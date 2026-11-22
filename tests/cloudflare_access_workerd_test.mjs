import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { Miniflare, convertV4MiniflareOptions } from '../deploy/cloudflare-worker/node_modules/miniflare/dist/src/index.js';

// Fixed-operation harness; never imported by or deployed with the production Worker.
const code = await readFile(new URL('../deploy/cloudflare-native/src/access.js', import.meta.url), 'utf8');
const script = `${code}\nexport default { async fetch(request, env) {
  const token = await createReadShare(env.DB, 'owner', 'resource', 1100, 1000);
  const hash = (await env.DB.prepare('SELECT token_hash FROM bearer_shares').first()).token_hash;
  return Response.json({ owner: await canAccess(env.DB, 'owner', 'resource', 'write'),
    other: await canAccess(env.DB, 'other', 'resource', 'read'),
    public: await canReadPublic(env.DB, 'resource'), token, hash,
    resolved: await resolveReadShare(env.DB, token, 1001),
    expired: await resolveReadShare(env.DB, token, 1100),
    unauthorized: await revokeReadShare(env.DB, 'other', 'resource', hash, 1001),
    revoked: await revokeReadShare(env.DB, 'owner', 'resource', hash, 1001),
    after: await resolveReadShare(env.DB, token, 1001) });
}};`;
const mf = new Miniflare(convertV4MiniflareOptions({ modules: true, script, compatibilityDate: '2026-11-16', d1Databases: ['DB'] }));
try {
  const db = await mf.getD1Database('DB');
  const migration = await readFile(new URL('../deploy/cloudflare-native/migrations/0001_access_foundation.sql', import.meta.url), 'utf8');
  const statements = migration.replace(/^\s*--.*$/gm, '').split(';').map(s => s.trim()).filter(Boolean);
  for (const sql of statements) await db.prepare(sql).run();
  await db.batch([
    db.prepare("INSERT INTO users (id, username) VALUES ('owner', 'owner'), ('other', 'other')"),
    db.prepare("INSERT INTO resources (id, owner_id) VALUES ('resource', 'owner')"),
  ]);
  const response = await mf.dispatchFetch('http://localhost/test');
  assert.equal(response.status, 200);
  const result = await response.json();
  assert.equal(result.owner, true);
  assert.equal(result.other, false);
  assert.equal(result.public, false);
  assert.match(result.token, /^[A-Za-z0-9_-]{43}$/);
  assert.match(result.hash, /^[a-f0-9]{64}$/);
  assert.notEqual(result.hash, result.token);
  assert.equal(result.resolved, 'resource');
  assert.equal(result.expired, null);
  assert.equal(result.unauthorized, false);
  assert.equal(result.revoked, true);
  assert.equal(result.after, null);
  console.log('Access foundation workerd and local D1 emulator tests passed');
} finally { await mf.dispose(); }
