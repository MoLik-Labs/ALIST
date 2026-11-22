import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { Miniflare, convertV4MiniflareOptions } from '../deploy/cloudflare-worker/node_modules/miniflare/dist/src/index.js';

// Fixed test operations only. Never used by the real Worker.
const code = await readFile(new URL('../deploy/cloudflare-native/src/sessions.js', import.meta.url), 'utf8');
const script = `${code}\nexport default { async fetch(request, env) {
  const token = await issueSession(env.DB, 'alice', 1100, 1000);
  const hash = (await env.DB.prepare('SELECT token_hash FROM sessions').first()).token_hash;
  const result = { token, hash, user: await authenticateSession(env.DB, token, 1001),
    expired: await authenticateSession(env.DB, token, 1100),
    revoked: await revokeSession(env.DB, token, 1002),
    after: await authenticateSession(env.DB, token, 1003) };
  return Response.json(result);
}};`;
const mf = new Miniflare(convertV4MiniflareOptions({ modules: true, script, compatibilityDate: '2026-11-16', d1Databases: ['DB'] }));
try {
  const db = await mf.getD1Database('DB');
  for (const name of ['0001_access_foundation.sql', '0002_sessions.sql']) {
    const migration = await readFile(new URL(`../deploy/cloudflare-native/migrations/${name}`, import.meta.url), 'utf8');
    const statements = migration.replace(/^\s*--.*$/gm, '').split(';').map(s => s.trim()).filter(Boolean);
    for (const sql of statements) await db.prepare(sql).run();
  }
  await db.prepare("INSERT INTO users (id, username) VALUES ('alice', 'alice')").run();
  const response = await mf.dispatchFetch('http://localhost/test');
  assert.equal(response.status, 200);
  const result = await response.json();
  assert.match(result.token, /^[A-Za-z0-9_-]{43}$/);
  assert.match(result.hash, /^[a-f0-9]{64}$/);
  assert.notEqual(result.hash, result.token);
  assert.equal(result.user, 'alice');
  assert.equal(result.expired, null);
  assert.equal(result.revoked, true);
  assert.equal(result.after, null);
  console.log('Session primitives workerd and local D1 emulator tests passed');
} finally { await mf.dispose(); }
