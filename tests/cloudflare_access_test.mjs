import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { DatabaseSync } from 'node:sqlite';
import { canAccess, canReadPublic, createReadShare, resolveReadShare, revokeReadShare } from '../deploy/cloudflare-native/src/access.js';

// SQLite-backed D1-shaped adapter; actual D1 runtime behavior is tested separately.
const sqlite = new DatabaseSync(':memory:');
sqlite.exec(await readFile(new URL('../deploy/cloudflare-native/migrations/0001_access_foundation.sql', import.meta.url), 'utf8'));
const db = { prepare(sql) { return { bind(...values) { return {
  async first() { return sqlite.prepare(sql).get(...values) ?? null; },
  async run() { return { meta: { changes: Number(sqlite.prepare(sql).run(...values).changes) } }; },
}; } }; } };
sqlite.exec(`INSERT INTO users (id, username, disabled) VALUES ('alice','alice',0), ('bob','bob',0), ('admin','admin',0), ('disabled','disabled',1);
  INSERT INTO roles VALUES ('readers','readers'), ('sharers','sharers');
  INSERT INTO user_roles VALUES ('bob','readers'), ('admin','sharers');
  INSERT INTO resources (id, owner_id) VALUES ('private','alice'), ('other','bob'), ('public','alice');
  UPDATE resources SET visibility='public' WHERE id='public';
  INSERT INTO resource_role_grants VALUES ('private','readers','read'), ('private','sharers','share');`);
assert.equal(await canReadPublic(db, 'private'), false);
assert.equal(await canReadPublic(db, 'public'), true);
for (const action of ['read', 'write', 'delete', 'share']) assert.equal(await canAccess(db, 'alice', 'private', action), true);
assert.equal(await canAccess(db, 'bob', 'private', 'read'), true);
assert.equal(await canAccess(db, 'bob', 'private', 'write'), false);
assert.equal(await canAccess(db, 'admin', 'private', 'read'), false, 'admin-like name must not bypass grants');
assert.equal(await canAccess(db, 'admin', 'private', 'share'), true);
assert.equal(await canAccess(db, 'disabled', 'private', 'read'), false);
assert.equal(await canAccess(db, 'alice', 'other', 'read'), false);
assert.equal(await canAccess(db, 'alice', 'private', 'bogus'), false);
assert.equal(await createReadShare(db, 'bob', 'private', 1100, 1000), null);
assert.equal(await createReadShare(db, 'alice', 'private', 1000, 1000), null);
assert.equal(await createReadShare(db, 'alice', 'private', 1000 + 31*86400, 1000), null);
const token = await createReadShare(db, 'admin', 'private', 1100, 1000);
assert.match(token, /^[A-Za-z0-9_-]{43}$/);
const row = sqlite.prepare('SELECT token_hash FROM bearer_shares').get();
assert.notEqual(row.token_hash, token);
assert.match(row.token_hash, /^[a-f0-9]{64}$/);
assert.equal(await resolveReadShare(db, token, 1099), 'private');
assert.equal(await resolveReadShare(db, token, 1100), null);
assert.equal(await resolveReadShare(db, token + 'x', 1001), null);
assert.equal(await revokeReadShare(db, 'bob', 'private', row.token_hash, 1001), false);
assert.equal(await revokeReadShare(db, 'alice', 'other', row.token_hash, 1001), false);
assert.equal(await revokeReadShare(db, 'alice', 'private', row.token_hash, 1001), true);
assert.equal(await revokeReadShare(db, 'alice', 'private', row.token_hash, 1001), false);
assert.equal(await resolveReadShare(db, token, 1001), null);
const token2 = await createReadShare(db, 'alice', 'private', 1100, 1000);
sqlite.exec("UPDATE users SET disabled=1 WHERE id='alice'");
assert.equal(await resolveReadShare(db, token2, 1001), null);
assert.equal(await canAccess(db, 'alice', 'private', 'read'), false);
sqlite.exec("UPDATE resources SET deleted_at=1001 WHERE id='public'");
assert.equal(await canReadPublic(db, 'public'), false);
sqlite.close();
console.log('Cloudflare access foundation SQLite tests passed');
