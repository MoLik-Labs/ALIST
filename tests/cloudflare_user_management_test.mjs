import assert from 'node:assert/strict';
import { DatabaseSync } from 'node:sqlite';
import { readFile } from 'node:fs/promises';
import { disableUser } from '../deploy/cloudflare-native/src/user-management.js';
import { issueSession, authenticateSession } from '../deploy/cloudflare-native/src/sessions.js';
import { createReadShare, resolveReadShare } from '../deploy/cloudflare-native/src/access.js';

const sqlite = new DatabaseSync(':memory:');
for (const name of ['0001_access_foundation.sql', '0002_sessions.sql', '0003_management_permissions.sql'])
  sqlite.exec(await readFile(new URL(`../deploy/cloudflare-native/migrations/${name}`, import.meta.url), 'utf8'));
const db = {
  prepare(sql) { return { bind(...values) { return {
    async first() { return sqlite.prepare(sql).get(...values) ?? null; },
    async run() { return { meta: { changes: Number(sqlite.prepare(sql).run(...values).changes) } }; },
  }; } }; },
  async batch(statements) {
    sqlite.exec('BEGIN');
    try { const results = []; for (const statement of statements) results.push(await statement.run()); sqlite.exec('COMMIT'); return results; }
    catch (error) { sqlite.exec('ROLLBACK'); throw error; }
  },
};
sqlite.exec(`INSERT INTO users (id,username,disabled) VALUES ('admin','admin',0),('alice','alice',0),('bob','bob',0),('disabled','disabled',1);
INSERT INTO roles VALUES ('manage','manage');
INSERT INTO user_roles VALUES ('admin','manage'),('disabled','manage');
INSERT INTO role_permissions VALUES ('manage','manage_users');
INSERT INTO resources (id,owner_id) VALUES ('file','alice');`);
const session = await issueSession(db, 'alice', 1100, 1000);
const share = await createReadShare(db, 'alice', 'file', 1100, 1000);
assert.equal(await disableUser(db, 'bob', 'alice', 1001), false);
assert.equal(await disableUser(db, 'disabled', 'alice', 1001), false);
assert.equal(await disableUser(db, 'admin', 'admin', 1001), false);
assert.equal(await disableUser(db, 'admin', 'missing', 1001), false);
assert.equal(await authenticateSession(db, session, 1002), 'alice');
assert.equal(await resolveReadShare(db, share, 1002), 'file');
assert.equal(await disableUser(db, 'admin', 'alice', 1001), true);
assert.equal(await authenticateSession(db, session, 1002), null);
assert.equal(await resolveReadShare(db, share, 1002), null);
sqlite.exec("UPDATE users SET disabled=0 WHERE id='alice'");
assert.equal(await authenticateSession(db, session, 1002), null, 'sessions must not reactivate after user re-enable');
assert.equal(await resolveReadShare(db, share, 1002), null, 'shares must not reactivate after user re-enable');
const laterSession = await issueSession(db, 'alice', 1200, 1005);
sqlite.exec("CREATE TRIGGER fail_revocation BEFORE UPDATE ON sessions BEGIN SELECT RAISE(ABORT, 'injected failure'); END");
await assert.rejects(disableUser(db, 'admin', 'alice', 1006));
assert.equal(sqlite.prepare("SELECT disabled FROM users WHERE id='alice'").get().disabled, 0, 'batch failure must roll back disable');
assert.equal(await authenticateSession(db, laterSession, 1007), 'alice');
sqlite.exec('DROP TRIGGER fail_revocation');
const emptyAdminDb = { batch() { throw new Error('unexpected database operation'); } };
assert.equal(await disableUser(emptyAdminDb, 'admin', 'admin', 1000), false);
assert.equal(await disableUser(emptyAdminDb, 'admin', 'bad id', 1000), false);
sqlite.close();
console.log('Cloudflare experimental atomic user-disable tests passed');
