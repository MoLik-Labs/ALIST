import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { Miniflare, convertV4MiniflareOptions } from '../deploy/cloudflare-worker/node_modules/miniflare/dist/src/index.js';
const code = await readFile(new URL('../deploy/cloudflare-native/src/role-management.js', import.meta.url), 'utf8');
const script = `${code}\nexport default { async fetch(request, env) {
  const denied = await removeResourceRoleGrant(env.DB, 'other', 'file', 'role', 'share', 1001);
  const allowed = await removeResourceRoleGrant(env.DB, 'owner', 'file', 'role', 'share', 1001);
  return Response.json({ denied, allowed,
    grant: await env.DB.prepare("SELECT action FROM resource_role_grants WHERE resource_id='file'").first(),
    share: await env.DB.prepare("SELECT revoked_at FROM bearer_shares WHERE created_by='sharer'").first() });
}};`;
const mf = new Miniflare(convertV4MiniflareOptions({ modules: true, script, compatibilityDate: '2026-11-16', d1Databases: ['DB'] }));
try {
  const db = await mf.getD1Database('DB');
  const migration = await readFile(new URL('../deploy/cloudflare-native/migrations/0001_access_foundation.sql', import.meta.url), 'utf8');
  for (const sql of migration.replace(/^\s*--.*$/gm, '').split(';').map(s => s.trim()).filter(Boolean)) await db.prepare(sql).run();
  await db.batch([
    db.prepare("INSERT INTO users(id,username) VALUES('owner','owner'),('sharer','sharer'),('other','other')"),
    db.prepare("INSERT INTO roles VALUES('role','role')"),
    db.prepare("INSERT INTO user_roles VALUES('sharer','role')"),
    db.prepare("INSERT INTO resources(id,owner_id) VALUES('file','owner')"),
    db.prepare("INSERT INTO resource_role_grants VALUES('file','role','share')"),
    db.prepare("INSERT INTO bearer_shares (token_hash,resource_id,created_by,expires_at,created_at) VALUES('share','file','sharer',1100,1000)"),
  ]);
  const response = await mf.dispatchFetch('http://localhost/test');
  assert.equal(response.status, 200);
  assert.deepEqual(await response.json(), { denied: false, allowed: true, grant: null, share: { revoked_at: 1001 } });
  console.log('Role grant workerd and local D1 emulator tests passed');
} finally { await mf.dispose(); }
