import assert from 'node:assert/strict';
import { DatabaseSync } from 'node:sqlite';
import { readFile } from 'node:fs/promises';
import { handleRequest } from '../deploy/cloudflare-native/src/index.js';
import { createGoogleConnection } from '../deploy/cloudflare-native/src/credential-vault.js';
import { issueSession, revokeSession } from '../deploy/cloudflare-native/src/sessions.js';

const sqlite = new DatabaseSync(':memory:');
for (const migration of ['0001_access_foundation.sql', '0002_sessions.sql', '0004_connections.sql'])
  sqlite.exec(await readFile(new URL(`../deploy/cloudflare-native/migrations/${migration}`, import.meta.url), 'utf8'));
const db = { prepare(sql) { return { bind(...values) { return {
  async first() { return sqlite.prepare(sql).get(...values) ?? null; },
  async all() { return { results: sqlite.prepare(sql).all(...values) }; },
  async run() { return { meta: { changes: Number(sqlite.prepare(sql).run(...values).changes) } }; },
}; } }; } };
sqlite.exec("INSERT INTO users(id,username) VALUES('alice','alice'),('bob','bob')");
const key = Buffer.from(crypto.getRandomValues(new Uint8Array(32))).toString('base64url');
const credentials = { clientId: 'test-client', clientSecret: 'test-secret', refreshToken: 'test-refresh' };
const connection = await createGoogleConnection(db, key, 'alice', credentials);
const bobConnection = await createGoogleConnection(db, key, 'bob', {clientId:'bob-client',clientSecret:'bob-secret',refreshToken:'bob-refresh'});
const alice = await issueSession(db, 'alice', Math.floor(Date.now()/1000)+600);
const bob = await issueSession(db, 'bob', Math.floor(Date.now()/1000)+600);
const env = { DB: db, KOVA_CREDENTIAL_KEY: key, KOVA_EXPERIMENTAL_V2: 'enabled' };
const request = (token, c = connection, path = '/v2/google-drive/files', method = 'GET') => new Request(`https://worker.test${path}?connection=${c}&parent=root`, { method, headers: token ? { authorization: `Bearer ${token}` } : {} });
const originalFetch = globalThis.fetch;
const requests = [];
globalThis.fetch = async (url, options) => {
  requests.push({ url: String(url), options });
  if (String(url) === 'https://oauth2.googleapis.com/token') return Response.json({ access_token: 'test-access' });
  return Response.json({ files: [{ id: 'doc', name: 'Document', mimeType: 'text/plain' }] });
};
try {
  assert.equal((await handleRequest(request(alice), { ...env, KOVA_EXPERIMENTAL_V2: undefined })).status, 404);
  assert.equal((await handleRequest(request(alice), { ...env, DB: undefined })).status, 503);
  assert.equal((await handleRequest(request(null), env)).status, 401);
  assert.equal((await handleRequest(request(alice, 'bad'), env)).status, 400);
  assert.equal((await handleRequest(request(bob), env)).status, 404, 'a different user cannot access this connection');
  assert.equal((await handleRequest(request(alice, connection, '/v2/google-drive/files', 'POST'), env)).status, 405);
  assert.equal((await handleRequest(request(alice, connection, '/v2/google-drive/bad'), env)).status, 404);
  assert.equal(requests.length, 0, 'unauthorized requests must never contact Google');
  let listing = await handleRequest(request(alice, connection, '/v2/connections'), env);
  assert.equal(listing.status, 200);
  assert.equal(listing.headers.get('cache-control'), 'no-store');
  const listText = await listing.text();
  const items = JSON.parse(listText).connections;
  assert.deepEqual(items.map(item => item.id), [connection]);
  assert.equal(items[0].provider, 'google_drive');
  assert.ok(!listText.includes(bobConnection) && !listText.includes('test-secret') && !listText.includes('ciphertext'));
  listing = await handleRequest(request(bob, connection, '/v2/connections'), env);
  assert.deepEqual((await listing.json()).connections.map(item => item.id), [bobConnection]);
  assert.equal((await handleRequest(new Request('https://worker.test/v2/connections?cursor=bad',{headers:{authorization:`Bearer ${alice}`}}),env)).status,400);
  assert.equal((await handleRequest(new Request('https://worker.test/v2/connections?cursor='+connection+'&cursor='+connection,{headers:{authorization:`Bearer ${alice}`}}),env)).status,400);
  const insert = sqlite.prepare("INSERT INTO provider_connections (id,owner_id,provider,vault_version,nonce,ciphertext,created_at) VALUES (?,'alice','google_drive',1,'synthetic','synthetic',1)");
  for (let i=0; i<52; i++) insert.run(`page${String(i).padStart(18,'0')}`);
  const page1 = await (await handleRequest(request(alice, connection, '/v2/connections'), env)).json();
  assert.equal(page1.connections.length, 50);
  assert.equal(page1.nextCursor, page1.connections.at(-1).id);
  const page2 = await (await handleRequest(new Request('https://worker.test/v2/connections?cursor='+page1.nextCursor,{headers:{authorization:`Bearer ${alice}`}}),env)).json();
  assert.equal(page2.connections.length, 3);
  assert.equal(page2.nextCursor, null);
  assert.equal(new Set([...page1.connections,...page2.connections].map(item=>item.id)).size,53);
  let response = await handleRequest(request(alice), env);
  assert.equal(response.status, 200);
  const body = await response.text();
  assert.match(body, /Document/);
  assert.ok(!body.includes('test-secret') && !body.includes('test-refresh') && !body.includes('test-access'));
  assert.equal(requests.length, 2);
  assert.equal(requests[0].url, 'https://oauth2.googleapis.com/token');
  assert.equal(requests[0].options.body.get('refresh_token'), 'test-refresh');
  assert.equal(requests[1].options.headers.authorization, 'Bearer test-access');
  assert.equal(new URL(requests[1].url).searchParams.has('connection'), false);
  response = await handleRequest(request(alice, connection, '/v2/me'), { ...env, KOVA_CREDENTIAL_KEY: undefined });
  assert.deepEqual(await response.json(), { id: 'alice' });
  assert.equal((await handleRequest(request(alice, connection, '/v2/me'), { ...env, DB: { prepare() { throw Error('secret-db-details'); } } })).status, 503);
  assert.equal((await handleRequest(request(null, connection, '/v2/me'), env)).status, 401);
  assert.equal((await handleRequest(request(bob, connection, '/v2/logout'), env)).status, 405);
  assert.equal((await handleRequest(request(bob, connection, '/v2/logout', 'POST'), env)).status, 200);
  assert.equal((await handleRequest(request(bob, connection, '/v2/me'), env)).status, 401);
  assert.equal((await handleRequest(request(alice, connection, '/v2/me'), env)).status, 200, 'logout must not revoke another user');
  await revokeSession(db, alice);
  assert.equal((await handleRequest(request(alice), env)).status, 401);
  assert.equal(requests.length, 2);
  const alice2 = await issueSession(db, 'alice', Math.floor(Date.now()/1000)+600);
  sqlite.exec("UPDATE users SET disabled=1 WHERE id='alice'");
  assert.equal((await handleRequest(request(alice2), env)).status, 401);
  assert.equal(requests.length, 2);
  console.log('Experimental connected-drive session isolation tests passed');
} finally { globalThis.fetch = originalFetch; sqlite.close(); }
