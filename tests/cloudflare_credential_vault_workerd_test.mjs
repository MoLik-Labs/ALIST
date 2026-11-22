import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { Miniflare, convertV4MiniflareOptions } from '../deploy/cloudflare-worker/node_modules/miniflare/dist/src/index.js';
const code = await readFile(new URL('../deploy/cloudflare-native/src/credential-vault.js', import.meta.url), 'utf8');
// Test-only harness with synthetic credentials and local emulator; never deploy.
const script = `${code}\nexport default { async fetch(request, env) {
  const key = '${Buffer.alloc(32, 7).toString('base64url')}';
  const creds = { clientId: 'local-client', clientSecret: 'local-secret', refreshToken: 'local-refresh' };
  const id = await createGoogleConnection(env.DB, key, 'alice', creds, 1000);
  const row = await env.DB.prepare('SELECT * FROM provider_connections WHERE id=?').bind(id).first();
  const read = await readGoogleConnection(env.DB, key, 'alice', id);
  const other = await readGoogleConnection(env.DB, key, 'bob', id);
  const deleted = await deleteGoogleConnection(env.DB, 'alice', id);
  const after = await readGoogleConnection(env.DB, key, 'alice', id);
  return Response.json({ id, row, read, other, deleted, after });
}};`;
const mf = new Miniflare(convertV4MiniflareOptions({ modules: true, script, compatibilityDate: '2026-11-16', d1Databases: ['DB'] }));
try {
  const db = await mf.getD1Database('DB');
  for (const name of ['0001_access_foundation.sql', '0004_connections.sql']) {
    const migration = await readFile(new URL(`../deploy/cloudflare-native/migrations/${name}`, import.meta.url), 'utf8');
    for (const sql of migration.replace(/^\s*--.*$/gm, '').split(';').map(s => s.trim()).filter(Boolean)) await db.prepare(sql).run();
  }
  await db.prepare("INSERT INTO users(id,username) VALUES('alice','alice'),('bob','bob')").run();
  const response = await mf.dispatchFetch('http://localhost/test');
  assert.equal(response.status, 200);
  const result = await response.json();
  assert.match(result.id, /^[A-Za-z0-9_-]{22}$/);
  assert.ok(!JSON.stringify(result.row).includes('local-secret'));
  assert.deepEqual(result.read, { clientId: 'local-client', clientSecret: 'local-secret', refreshToken: 'local-refresh' });
  assert.equal(result.other, null);
  assert.equal(result.deleted, true);
  assert.equal(result.after, null);
  console.log('Credential vault workerd and local D1 emulator tests passed');
} finally { await mf.dispose(); }
