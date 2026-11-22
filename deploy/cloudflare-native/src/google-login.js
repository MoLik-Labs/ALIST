// Experimental, disabled-by-default Google OIDC login. No production rollout.
// Account identities must be provisioned separately; no public signup.
import { issueSession } from './sessions.js';
import { verifyGoogleIdentityToken } from './google-identity.js';
import { vaultKey, decode, readSmallJson } from './google-oauth.js';
const enc = new TextEncoder();
const b64 = bytes => btoa(String.fromCharCode(...bytes)).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
const sha = async value => new Uint8Array(await crypto.subtle.digest('SHA-256', enc.encode(value)));
const hex = bytes => [...bytes].map(n => n.toString(16).padStart(2, '0')).join('');
const rand = () => b64(crypto.getRandomValues(new Uint8Array(32)));
const nowSeconds = () => Math.floor(Date.now()/1000);
const tokenPattern = /^[A-Za-z0-9_-]{43}$/;
const cookieName = '__Host-kova_login';
const secureCookie = (value, maxAge) => `${cookieName}=${value}; Path=/; Max-Age=${maxAge}; Secure; HttpOnly; SameSite=Lax`;
function json(status, payload, cookie) {
  const headers = { 'content-type':'application/json; charset=utf-8', 'cache-control':'no-store', 'x-content-type-options':'nosniff', 'referrer-policy':'no-referrer', 'content-security-policy':"default-src 'none'; frame-ancestors 'none'" };
  if (cookie) headers['set-cookie'] = cookie;
  return new Response(JSON.stringify(payload), { status, headers });
}
const fail = (status, message) => json(status, { error:message });
const secret = x => typeof x === 'string' && x.length > 0 && x.length <= 4096 && !/[\x00-\x20\x7f]/.test(x);
function browserCookie(request) {
  const matches = (request.headers.get('cookie') || '').split(';').map(s => s.trim()).filter(s => s.startsWith(`${cookieName}=`));
  if (matches.length !== 1) return null;
  const value = matches[0].slice(cookieName.length + 1);
  return tokenPattern.test(value) ? value : null;
}
function config(env, request) {
  if (!env?.DB || !secret(env.GOOGLE_CLIENT_ID) || !secret(env.GOOGLE_CLIENT_SECRET) || typeof env.KOVA_CREDENTIAL_KEY !== 'string' || typeof env.KOVA_LOGIN_REDIRECT !== 'string') return null;
  try {
    const callback = new URL(env.KOVA_LOGIN_REDIRECT), incoming = new URL(request.url);
    if (callback.protocol !== 'https:' || callback.username || callback.password || callback.search || callback.hash || callback.pathname !== '/v2/auth/google/callback' || callback.origin !== incoming.origin) return null;
    return callback;
  } catch { return null; }
}
const aad = (stateHash, browserHash) => enc.encode(JSON.stringify([1,'google-login',stateHash,browserHash]));

export async function googleLoginRequest(request, env) {
  if (env?.KOVA_EXPERIMENTAL_V2 !== 'enabled' || env?.KOVA_EXPERIMENTAL_LOGIN !== 'enabled') return fail(404,'Not found');
  const url = new URL(request.url);
  const start = url.pathname === '/v2/auth/google/start', callback = url.pathname === '/v2/auth/google/callback';
  if (!start && !callback) return fail(404,'Not found');
  if ((start && request.method !== 'POST') || (callback && request.method !== 'GET')) return fail(405,'Method not allowed');
  const redirect = config(env, request);
  if (!redirect) return fail(503,'Login is not configured');
  if (start && (request.headers.get('origin') !== redirect.origin || !/^application\/json(?:\s*;|$)/i.test(request.headers.get('content-type') || ''))) return fail(403,'Invalid request origin');
  try {
    const key = await vaultKey(env.KOVA_CREDENTIAL_KEY), now = nowSeconds();
    if (start) {
      const state = rand(), browser = rand(), verifier = rand(), nonce = rand();
      const stateHash = hex(await sha(state)), browserHash = hex(await sha(browser));
      const iv = crypto.getRandomValues(new Uint8Array(12));
      const ciphertext = new Uint8Array(await crypto.subtle.encrypt({ name:'AES-GCM', iv, additionalData:aad(stateHash,browserHash) },key,enc.encode(verifier)));
      // Prototype cap protects against unlimited active state rows; it is not a
      // production rate limit and can itself be exhausted by unauthenticated users.
      await env.DB.prepare('DELETE FROM oidc_login_pending WHERE state_hash IN (SELECT state_hash FROM oidc_login_pending WHERE expires_at <= ? LIMIT 100)').bind(now).run();
      const inserted = await env.DB.prepare(`INSERT INTO oidc_login_pending (state_hash,browser_hash,nonce,verifier_nonce,verifier_ciphertext,created_at,expires_at)
        SELECT ?,?,?,?,?,?,? WHERE (SELECT COUNT(*) FROM oidc_login_pending WHERE expires_at > ?) < 100`)
        .bind(stateHash,browserHash,nonce,b64(iv),b64(ciphertext),now,now+600,now).run();
      if (inserted.meta?.changes !== 1) return fail(429,'Login temporarily unavailable');
      const target = new URL('https://accounts.google.com/o/oauth2/v2/auth');
      target.search = new URLSearchParams({client_id:env.GOOGLE_CLIENT_ID,redirect_uri:redirect.toString(),response_type:'code',scope:'openid',state,nonce,code_challenge:b64(await sha(verifier)),code_challenge_method:'S256'}).toString();
      return json(200,{authorizationUrl:target.toString()},secureCookie(browser,600));
    }
    const states = url.searchParams.getAll('state'), codes = url.searchParams.getAll('code'), browser = browserCookie(request);
    if (states.length !== 1 || codes.length !== 1 || !tokenPattern.test(states[0]) || !secret(codes[0]) || !browser) return fail(400,'Invalid or expired login');
    const stateHash = hex(await sha(states[0])), browserHash = hex(await sha(browser));
    const pending = await env.DB.prepare(`DELETE FROM oidc_login_pending WHERE state_hash = ? AND browser_hash = ? AND expires_at > ?
      RETURNING nonce,verifier_nonce,verifier_ciphertext`).bind(stateHash,browserHash,now).first();
    if (!pending) return fail(400,'Invalid or expired login');
    const iv = decode(pending.verifier_nonce,16);
    if (iv.length !== 12 || !tokenPattern.test(pending.nonce)) throw Error('Invalid state');
    const verifier = new TextDecoder('utf-8',{fatal:true}).decode(await crypto.subtle.decrypt({name:'AES-GCM',iv,additionalData:aad(stateHash,browserHash)},key,decode(pending.verifier_ciphertext,256)));
    if (!tokenPattern.test(verifier)) throw Error('Invalid state');
    const tokenResponse = await fetch('https://oauth2.googleapis.com/token',{method:'POST',redirect:'manual',signal:AbortSignal.timeout(15000),headers:{'content-type':'application/x-www-form-urlencoded'},body:new URLSearchParams({code:codes[0],client_id:env.GOOGLE_CLIENT_ID,client_secret:env.GOOGLE_CLIENT_SECRET,redirect_uri:redirect.toString(),grant_type:'authorization_code',code_verifier:verifier})});
    if (tokenResponse.status !== 200) return fail(502,'Google login failed');
    const token = (await readSmallJson(tokenResponse)).id_token;
    const subject = await verifyGoogleIdentityToken(token,env.GOOGLE_CLIENT_ID,pending.nonce,nowSeconds());
    if (!subject) return fail(401,'Login failed');
    const row = await env.DB.prepare(`SELECT u.id FROM external_identities AS e JOIN users AS u ON u.id=e.user_id AND u.disabled=0
      WHERE e.provider='google' AND e.subject=? LIMIT 1`).bind(subject).first();
    if (!row) return fail(403,'Account is not authorized');
    const issuedAt = nowSeconds();
    const session = await issueSession(env.DB,row.id,issuedAt+3600,issuedAt,{provider:'google',subject});
    if (!session) return fail(403,'Account is not authorized');
    return json(200,{token:session},secureCookie('',0));
  } catch { return fail(503,'Experimental login unavailable'); }
}
