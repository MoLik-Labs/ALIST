import assert from 'node:assert/strict';
import { DatabaseSync } from 'node:sqlite';
import { readFile } from 'node:fs/promises';
import { issueSession, authenticateSession, revokeSession, revokeAllSessionsForUser } from '../deploy/cloudflare-native/src/sessions.js';

const sqlite = new DatabaseSync(':memory:');
for (const migration of ['0001_access_foundation.sql', '0002_sessions.sql']) {
  sqlite.exec(await readFile(new URL(`../deploy/cloudflare-native/migrations/${migration}`, import.meta.url), 'utf8'));
}
const db = { prepare(sql) { return { bind(...values) { return {
  async first() { return sqlite.prepare(sql).get(...values) ?? null; },
  async run() { return { meta: { changes: Number(sqlite.prepare(sql).run(...values).changes) } }; },
}; } }; } };
sqlite.exec("INSERT INTO users (id, username, disabled) VALUES ('alice','alice',0), ('bob','bob',0), ('disabled','disabled',1)");
assert.equal(await issueSession(db, 'missing', 1100, 1000), null);
assert.equal(await issueSession(db, 'disabled', 1100, 1000), null);
assert.equal(await issueSession(db, 'alice', 1000, 1000), null);
assert.equal(await issueSession(db, 'alice', 1000+86401, 1000), null);
assert.equal(await issueSession(db, 'alice', 1001, -1), null);
const a = await issueSession(db, 'alice', 1100, 1000);
const b = await issueSession(db, 'alice', 1100, 1000);
const c = await issueSession(db, 'bob', 1100, 1000);
assert.match(a, /^[A-Za-z0-9_-]{43}$/);
assert.notEqual(a, b);
assert.ok(!JSON.stringify(sqlite.prepare('SELECT * FROM sessions').all()).includes(a));
assert.equal(await authenticateSession(db, a, 1001), 'alice');
assert.equal(await authenticateSession(db, a, 1100), null);
assert.equal(await authenticateSession(db, a + 'x', 1001), null);
assert.equal(await revokeSession(db, a, 1001), true);
assert.equal(await revokeSession(db, a, 1001), false);
assert.equal(await authenticateSession(db, a, 1002), null);
assert.equal(await authenticateSession(db, b, 1002), 'alice');
assert.equal(await revokeAllSessionsForUser(db, 'alice', 1002), true);
assert.equal(await authenticateSession(db, b, 1003), null);
assert.equal(await authenticateSession(db, c, 1003), 'bob');
sqlite.exec("UPDATE users SET disabled=1 WHERE id='bob'");
assert.equal(await authenticateSession(db, c, 1003), null);
assert.equal(await issueSession(db, 'bob', 1100, 1003), null);
// Disabling a user must be paired with explicit permanent revocation: simply
// enabling the user again otherwise makes still-unexpired sessions usable.
assert.equal(await revokeAllSessionsForUser(db, 'bob', 1003), true);
sqlite.exec("UPDATE users SET disabled=0 WHERE id='bob'");
assert.equal(await authenticateSession(db, c, 1004), null);
sqlite.close();
console.log('Cloudflare experimental session SQLite tests passed');
