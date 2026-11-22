import assert from 'node:assert/strict';
import { DatabaseSync } from 'node:sqlite';
import { readFile } from 'node:fs/promises';
import { handleRequest } from '../deploy/cloudflare-native/src/index.js';
import { issueSession, revokeSession } from '../deploy/cloudflare-native/src/sessions.js';
import { readGoogleConnection } from '../deploy/cloudflare-native/src/credential-vault.js';

const sqlite = new DatabaseSync(':memory:');
for (const name of ['0001_access_foundation.sql','0002_sessions.sql','0004_connections.sql','0005_oauth_pending.sql']) sqlite.exec(await readFile(new URL(`../deploy/cloudflare-native/migrations/${name}`, import.meta.url), 'utf8'));
const db = { prepare(sql) { return { bind(...values) { return {
  async first() { return sqlite.prepare(sql).get(...values) ?? null; },
  async run() { return { meta: { changes: Number(sqlite.prepare(sql).run(...values).changes) } }; },
}; } }; } };
sqlite.exec("INSERT INTO users(id,username) VALUES('alice','alice'),('bob','bob')");
const key = Buffer.from(crypto.getRandomValues(new Uint8Array(32))).toString('base64url');
const env = { DB:db, KOVA_CREDENTIAL_KEY:key, KOVA_EXPERIMENTAL_V2:'enabled', KOVA_EXPERIMENTAL_OAUTH:'enabled', KOVA_GOOGLE_REDIRECT:'https://worker.test/v2/google-drive/oauth/callback', GOOGLE_CLIENT_ID:'test-client', GOOGLE_CLIENT_SECRET:'test-client-secret' };
const session = await issueSession(db, 'alice', Math.floor(Date.now()/1000)+600);
const start = token => new Request('https://worker.test/v2/google-drive/oauth/start', { method:'POST', headers: token ? { authorization:`Bearer ${token}` } : {} });
const callback = (state, code='example-code') => new Request(`https://worker.test/v2/google-drive/oauth/callback?state=${encodeURIComponent(state)}&code=${encodeURIComponent(code)}`);
const originalFetch = globalThis.fetch;
const requests = [];
let onExchange = async () => {};
globalThis.fetch = async (url, options) => { requests.push({ url: String(url), options }); await onExchange(); return Response.json({ refresh_token:'synthetic-refresh', access_token:'synthetic-access' }); };
try {
  assert.equal((await handleRequest(start(session), { ...env, KOVA_EXPERIMENTAL_OAUTH:undefined })).status, 404);
  assert.equal((await handleRequest(start(session), { ...env, KOVA_GOOGLE_REDIRECT:'http://worker.test/v2/google-drive/oauth/callback' })).status, 503);
  assert.equal((await handleRequest(start(null), env)).status, 401);
  assert.equal((await handleRequest(callback('invalid'), env)).status, 400);
  assert.equal(requests.length, 0);
  const begin = await handleRequest(start(session), env);
  assert.equal(begin.status, 200);
  assert.equal(begin.headers.get('cache-control'), 'no-store');
  assert.equal(begin.headers.get('referrer-policy'), 'no-referrer');
  const auth = new URL((await begin.json()).authorizationUrl);
  assert.equal(auth.origin, 'https://accounts.google.com');
  assert.equal(auth.searchParams.get('redirect_uri'), env.KOVA_GOOGLE_REDIRECT);
  assert.equal(auth.searchParams.get('code_challenge_method'), 'S256');
  assert.equal(auth.searchParams.get('scope'), 'https://www.googleapis.com/auth/drive.readonly');
  const state = auth.searchParams.get('state');
  assert.match(state, /^[A-Za-z0-9_-]{43}$/);
  const pending = sqlite.prepare('SELECT * FROM google_oauth_pending').get();
  assert.notEqual(pending.state_hash, state);
  assert.ok(!JSON.stringify(pending).includes(state));
  assert.ok(!JSON.stringify(pending).includes(session));
  const finish = await handleRequest(callback(state), env);
  assert.equal(finish.status, 201);
  const text = await finish.text();
  assert.ok(!text.includes('synthetic-refresh') && !text.includes('test-client-secret'));
  const id = JSON.parse(text).connection;
  assert.deepEqual(await readGoogleConnection(db, key, 'alice', id), {clientId:'test-client',clientSecret:'test-client-secret',refreshToken:'synthetic-refresh'});
  assert.equal(await readGoogleConnection(db, key, 'bob', id), null);
  assert.equal(requests.length, 1);
  assert.equal(requests[0].url, 'https://oauth2.googleapis.com/token');
  assert.equal(requests[0].options.redirect, 'manual');
  const verifier = requests[0].options.body.get('code_verifier');
  const challenge = Buffer.from(await crypto.subtle.digest('SHA-256', new TextEncoder().encode(verifier))).toString('base64url');
  assert.equal(challenge, auth.searchParams.get('code_challenge'));
  assert.equal((await handleRequest(callback(state), env)).status, 400, 'state must be single-use');
  assert.equal(requests.length, 1);
  const expired = await handleRequest(start(session), env);
  const expiredState = new URL((await expired.json()).authorizationUrl).searchParams.get('state');
  // Adjust creation and expiry together to satisfy the schema constraint.
  sqlite.prepare('UPDATE google_oauth_pending SET created_at=1,expires_at=2 WHERE user_id=?').run('alice');
  assert.equal((await handleRequest(callback(expiredState), env)).status, 400);
  assert.equal(requests.length, 1);
  const begin2 = await handleRequest(start(session), env);
  const state2 = new URL((await begin2.json()).authorizationUrl).searchParams.get('state');
  onExchange = () => revokeSession(db, session);
  assert.equal((await handleRequest(callback(state2), env)).status, 401, 'revocation during exchange prevents connection creation');
  assert.equal(sqlite.prepare('SELECT count(*) AS count FROM provider_connections').get().count, 1);
  const bob = await issueSession(db, 'bob', Math.floor(Date.now()/1000)+600);
  for (let i=0; i<5; i++) assert.equal((await handleRequest(start(bob), env)).status, 200);
  assert.equal((await handleRequest(start(bob), env)).status, 429);
  console.log('Experimental Google OAuth PKCE flow tests passed');
} finally { globalThis.fetch = originalFetch; sqlite.close(); }
