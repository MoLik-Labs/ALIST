import assert from 'node:assert/strict';
import { DatabaseSync } from 'node:sqlite';
import { readFile } from 'node:fs/promises';
import { createReadShare, resolveReadShare } from '../deploy/cloudflare-native/src/access.js';
import { removeResourceRoleGrant } from '../deploy/cloudflare-native/src/role-management.js';
const sqlite = new DatabaseSync(':memory:');
sqlite.exec(await readFile(new URL('../deploy/cloudflare-native/migrations/0001_access_foundation.sql', import.meta.url), 'utf8'));
const db = {
  prepare(sql) { return { bind(...values) { return {
    async first() { return sqlite.prepare(sql).get(...values) ?? null; },
    async run() { return { meta: { changes: Number(sqlite.prepare(sql).run(...values).changes) } }; },
  }; } }; },
  async batch(statements) { sqlite.exec('BEGIN'); try { const out = []; for (const s of statements) out.push(await s.run()); sqlite.exec('COMMIT'); return out; } catch (e) { sqlite.exec('ROLLBACK'); throw e; } },
};
sqlite.exec(`INSERT INTO users(id,username) VALUES('owner','owner'),('sharer','sharer'),('other','other');
INSERT INTO roles VALUES('role','role'); INSERT INTO user_roles VALUES('sharer','role');
INSERT INTO resources(id,owner_id) VALUES('file','owner');
INSERT INTO resource_role_grants VALUES('file','role','share'),('file','role','read');`);
const link = await createReadShare(db, 'sharer', 'file', 1100, 1000);
assert.equal(await removeResourceRoleGrant(db, 'other', 'file', 'role', 'share', 1001), false);
assert.equal(await resolveReadShare(db, link, 1002), 'file');
assert.equal(await removeResourceRoleGrant(db, 'owner', 'file', 'role', 'read', 1001), true);
assert.equal(await resolveReadShare(db, link, 1002), 'file');
// Inject a failure after the share update: both changes must roll back.
sqlite.exec("CREATE TRIGGER fail_grant BEFORE DELETE ON resource_role_grants BEGIN SELECT RAISE(ABORT, 'injected failure'); END");
await assert.rejects(removeResourceRoleGrant(db, 'owner', 'file', 'role', 'share', 1002));
assert.equal(await resolveReadShare(db, link, 1002), 'file');
sqlite.exec('DROP TRIGGER fail_grant');
assert.equal(await removeResourceRoleGrant(db, 'owner', 'file', 'role', 'share', 1002), true);
assert.equal(await resolveReadShare(db, link, 1002), null);
sqlite.exec("INSERT INTO resource_role_grants VALUES('file','role','share')");
assert.equal(await resolveReadShare(db, link, 1002), null, 'grant restoration must not revive revoked links');
assert.equal(await removeResourceRoleGrant(db, 'owner', 'file', 'role', 'invalid', 1002), false);
sqlite.close();
console.log('Cloudflare experimental role-grant revocation tests passed');
