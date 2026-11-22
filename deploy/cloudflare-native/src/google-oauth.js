// Experimental Google Drive OAuth connection flow. Disabled by default; not a
// login system or production-ready OAuth implementation. No HTML/JS is served.
import { authenticateSession } from './sessions.js';
import { createGoogleConnection } from './credential-vault.js';
const authUrl = 'https://accounts.google.com/o/oauth2/v2/auth';
const tokenUrl = 'https://oauth2.googleapis.com/token';
const enc = new TextEncoder();
const statePattern = /^[A-Za-z0-9_-]{43}$/;
const b64 = bytes => btoa(String.fromCharCode(...bytes)).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
const sha = async value => new Uint8Array(await crypto.subtle.digest('SHA-256', typeof value === 'string' ? enc.encode(value) : value));
const hex = bytes => [...bytes].map(b => b.toString(16).padStart(2, '0')).join('');
const rand = () => b64(crypto.getRandomValues(new Uint8Array(32)));
const json = (status, payload) => new Response(JSON.stringify(payload), { status, headers: {
  'content-type': 'application/json; charset=utf-8', 'cache-control': 'no-store',
  'x-content-type-options': 'nosniff', 'referrer-policy': 'no-referrer',
} });
const fail = (status, message) => json(status, { error: message });
const validSecret = x => typeof x === 'string' && x.length > 0 && x.length <= 4096 && !/[\x00-\x20\x7f]/.test(x);
const nowSeconds = () => Math.floor(Date.now() / 1000);
function decode(value, max) {
  if (typeof value !== 'string' || !/^[A-Za-z0-9_-]+$/.test(value) || value.length > max) throw Error('Invalid data');
  const input = value.replace(/-/g, '+').replace(/_/g, '/');
  const bytes = Uint8Array.from(atob(input + '='.repeat((4 - input.length % 4) % 4)), c => c.charCodeAt(0));
  if (b64(bytes) !== value) throw Error('Invalid data');
  return bytes;
}
async function vaultKey(value) {
  const raw = decode(value, 43);
  if (raw.length !== 32) throw Error('Invalid key');
  return crypto.subtle.importKey('raw', raw, 'AES-GCM', false, ['encrypt', 'decrypt']);
}
function redirectUri(raw) {
  if (typeof raw !== 'string' || raw.length > 2048) return null;
  try {
    const url = new URL(raw);
    if (url.protocol !== 'https:' || url.username || url.password || url.search || url.hash || url.pathname !== '/v2/google-drive/oauth/callback') return null;
    return url.toString();
  } catch { return null; }
}
const aad = (stateHash, userId, sessionHash) => enc.encode(JSON.stringify([1, 'google-oauth', stateHash, userId, sessionHash]));
async function readSmallJson(response) {
  if (!response.body) throw Error('Missing body');
  const reader = response.body.getReader();
  const parts = []; let total = 0;
  try {
    while (true) {
      const { done, value } = await reader.read();
      if (done) break;
      total += value.byteLength;
      if (total > 65536) { await reader.cancel().catch(() => {}); throw Error('Body too large'); }
      parts.push(value);
    }
  } finally { reader.releaseLock(); }
  const bytes = new Uint8Array(total); let offset = 0;
  for (const part of parts) { bytes.set(part, offset); offset += part.byteLength; }
  return JSON.parse(new TextDecoder('utf-8', { fatal: true }).decode(bytes));
}

export async function googleOauthRequest(request, env) {
  if (env?.KOVA_EXPERIMENTAL_V2 !== 'enabled' || env?.KOVA_EXPERIMENTAL_OAUTH !== 'enabled') return fail(404, 'Not found');
  const url = new URL(request.url);
  const start = url.pathname === '/v2/google-drive/oauth/start';
  const callback = url.pathname === '/v2/google-drive/oauth/callback';
  if (!start && !callback) return fail(404, 'Not found');
  if ((start && request.method !== 'POST') || (callback && request.method !== 'GET')) return fail(405, 'Method not allowed');
  const redirect = redirectUri(env.KOVA_GOOGLE_REDIRECT);
  if (!env.DB || !validSecret(env.GOOGLE_CLIENT_ID) || !validSecret(env.GOOGLE_CLIENT_SECRET) || !redirect || typeof env.KOVA_CREDENTIAL_KEY !== 'string') return fail(503, 'OAuth is not configured');
  try {
    const key = await vaultKey(env.KOVA_CREDENTIAL_KEY);
    const now = nowSeconds();
    if (start) {
      const match = /^Bearer ([A-Za-z0-9_-]{43})$/.exec(request.headers.get('authorization') || '');
      if (!match) return fail(401, 'Unauthorized');
      const userId = await authenticateSession(env.DB, match[1], now);
      if (!userId) return fail(401, 'Unauthorized');
      const sessionHash = hex(await sha(match[1]));
      const state = rand(), verifier = rand(), stateHash = hex(await sha(state));
      const nonce = crypto.getRandomValues(new Uint8Array(12));
      const sealed = new Uint8Array(await crypto.subtle.encrypt({ name: 'AES-GCM', iv: nonce, additionalData: aad(stateHash, userId, sessionHash) }, key, enc.encode(verifier)));
      // Opportunistic per-user cleanup; a production implementation also needs
      // durable rate limits and a bounded global expiry cleanup strategy.
      await env.DB.prepare('DELETE FROM google_oauth_pending WHERE user_id = ? AND expires_at <= ?').bind(userId, now).run();
      const inserted = await env.DB.prepare(`INSERT INTO google_oauth_pending (state_hash,user_id,session_hash,verifier_nonce,verifier_ciphertext,expires_at,created_at)
        SELECT ?,u.id,s.token_hash,?,?,?,? FROM users AS u JOIN sessions AS s ON s.user_id = u.id
        WHERE u.id = ? AND u.disabled = 0 AND s.token_hash = ? AND s.revoked_at IS NULL AND s.expires_at > ?
        AND (SELECT COUNT(*) FROM google_oauth_pending WHERE user_id = u.id AND expires_at > ?) < 5`)
        .bind(stateHash, b64(nonce), b64(sealed), now + 600, now, userId, sessionHash, now, now).run();
      if (inserted.meta?.changes !== 1) return fail(429, 'Unable to start another connection');
      const target = new URL(authUrl);
      target.search = new URLSearchParams({ client_id: env.GOOGLE_CLIENT_ID, redirect_uri: redirect, response_type: 'code', scope: 'https://www.googleapis.com/auth/drive.readonly', access_type: 'offline', prompt: 'consent', state, code_challenge: b64(await sha(verifier)), code_challenge_method: 'S256' }).toString();
      return json(200, { authorizationUrl: target.toString() });
    }
    const states = url.searchParams.getAll('state'), codes = url.searchParams.getAll('code');
    if (states.length !== 1 || codes.length !== 1 || !statePattern.test(states[0]) || !validSecret(codes[0])) return fail(400, 'Invalid or expired connection request');
    const stateHash = hex(await sha(states[0]));
    // A single SQL statement consumes the state once and verifies the initiating
    // session is still live. A failed token exchange requires a fresh attempt.
    const pending = await env.DB.prepare(`DELETE FROM google_oauth_pending WHERE state_hash = ? AND expires_at > ? AND EXISTS (
      SELECT 1 FROM sessions AS s JOIN users AS u ON u.id = s.user_id AND u.disabled = 0
      WHERE s.token_hash = google_oauth_pending.session_hash AND s.user_id = google_oauth_pending.user_id
        AND s.revoked_at IS NULL AND s.expires_at > ?)
      RETURNING user_id,session_hash,verifier_nonce,verifier_ciphertext`).bind(stateHash, now, now).first();
    if (!pending) return fail(400, 'Invalid or expired connection request');
    const nonce = decode(pending.verifier_nonce, 16);
    if (nonce.length !== 12) throw Error('Invalid data');
    const verifier = new TextDecoder('utf-8', { fatal: true }).decode(await crypto.subtle.decrypt({ name: 'AES-GCM', iv: nonce, additionalData: aad(stateHash, pending.user_id, pending.session_hash) }, key, decode(pending.verifier_ciphertext, 256)));
    if (!statePattern.test(verifier)) throw Error('Invalid data');
    const upstream = await fetch(tokenUrl, { method: 'POST', redirect: 'manual', signal: AbortSignal.timeout(15000), headers: { 'content-type': 'application/x-www-form-urlencoded' }, body: new URLSearchParams({ code: codes[0], client_id: env.GOOGLE_CLIENT_ID, client_secret: env.GOOGLE_CLIENT_SECRET, redirect_uri: redirect, grant_type: 'authorization_code', code_verifier: verifier }) });
    if (!upstream.ok) return fail(502, 'Google authorization failed');
    const result = await readSmallJson(upstream);
    if (!validSecret(result.refresh_token)) return fail(502, 'Google authorization did not return an offline token');
    const connection = await createGoogleConnection(env.DB, env.KOVA_CREDENTIAL_KEY, pending.user_id, { clientId: env.GOOGLE_CLIENT_ID, clientSecret: env.GOOGLE_CLIENT_SECRET, refreshToken: result.refresh_token }, nowSeconds(), pending.session_hash);
    if (!connection) return fail(401, 'Session expired or revoked');
    return json(201, { connection });
  } catch {
    return fail(503, 'Experimental OAuth unavailable');
  }
}
