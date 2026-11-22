import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { Miniflare, convertV4MiniflareOptions } from '../deploy/cloudflare-worker/node_modules/miniflare/dist/src/index.js';

// Test-only harness and ephemeral D1 emulator; never deploy this script.
const code = await readFile(new URL('../deploy/cloudflare-native/src/user-management.js', import.meta.url), 'utf8');
const script = `${code}\nexport default { async fetch(request, env) {
  const denied = await disableUser(env.DB, 'bob', 'alice', 1001);
  const allowed = await disableUser(env.DB, 'admin', 'alice', 1001);
  return Response.json({ denied, allowed,
    user: await env.DB.prepare("SELECT disabled FROM users WHERE id='alice'").first(),
    session: await env.DB.prepare("SELECT revoked_at FROM sessions WHERE user_id='alice'").first(),
    share: await env.DB.prepare("SELECT revoked_at FROM bearer_shares WHERE created_by='alice'").first() });
}};`;
const mf = new Miniflare(convertV4MiniflareOptions({ modules: true, script, compatibilityDate: '2026-11-16', d1Databases: ['DB'] }));
try {
  const db = await mf.getD1Database('DB');
  for (const name of ['0001_access_foundation.sql', '0002_sessions.sql', '0003_management_permissions.sql']) {
    const migration = await readFile(new URL(`../deploy/cloudflare-native/migrations/${name}`, import.meta.url), 'utf8');
    for (const sql of migration.replace(/^\s*--.*$/gm, '').split(';').map(s => s.trim()).filter(Boolean)) await db.prepare(sql).run();
  }
  await db.batch([
    db.prepare("INSERT INTO users (id,username) VALUES ('admin','admin'),('alice','alice'),('bob','bob')"),
    db.prepare("INSERT INTO roles VALUES ('manage','manage')"),
    db.prepare("INSERT INTO user_roles VALUES ('admin','manage')"),
    db.prepare("INSERT INTO role_permissions VALUES ('manage','manage_users')"),
    db.prepare("INSERT INTO resources (id,owner_id) VALUES ('file','alice')"),
    db.prepare("INSERT INTO sessions (token_hash,user_id,created_at,expires_at) VALUES ('token','alice',1000,1100)"),
    db.prepare("INSERT INTO bearer_shares (token_hash,resource_id,created_by,expires_at,created_at) VALUES ('share','file','alice',1100,1000)"),
  ]);
  const response = await mf.dispatchFetch('http://localhost/test');
  assert.equal(response.status, 200);
  assert.deepEqual(await response.json(), { denied: false, allowed: true, user: { disabled: 1 }, session: { revoked_at: 1001 }, share: { revoked_at: 1001 } });
  console.log('User management workerd and local D1 emulator tests passed');
} finally { await mf.dispose(); }
