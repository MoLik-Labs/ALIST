import assert from 'node:assert/strict';
import { handleRequest } from '../deploy/cloudflare-native/src/index.js';

const secret = 'test-only-secret-at-least-32-characters';
const seen = [];
const bucket = {
  async list(options) { seen.push(options); return { objects: [{ key: 'docs/a.txt', size: 3, uploaded: new Date('2026-01-01T00:00:00Z') }], truncated: true, cursor: 'next' }; },
  async get(key) { seen.push(key); return key === 'docs/a.txt' ? { size: 3, body: new Response('abc').body } : null; },
};
const env = { R2: bucket, KOVA_NATIVE_TOKEN: secret };
const req = (path, token = secret, method = 'GET') => new Request('https://native.test' + path, { method, headers: { authorization: `Bearer ${token}` } });
assert.equal((await handleRequest(req('/ping'), {})).status, 200);
assert.equal((await handleRequest(req('/v1/objects'), {})).status, 503);
assert.equal((await handleRequest(req('/v1/objects', 'wrong'), env)).status, 401);
assert.equal(seen.length, 0, 'unauthorized requests must not reach storage');
assert.equal((await handleRequest(req('/v1/objects', secret, 'POST'), env)).status, 405);
const listing = await handleRequest(req('/v1/objects?prefix=docs%2F&cursor=opaque'), env);
assert.equal(listing.status, 200);
assert.equal(listing.headers.get('cache-control'), 'no-store');
assert.deepEqual(await listing.json(), { objects: [{ key: 'docs/a.txt', size: 3, uploaded: '2026-01-01T00:00:00.000Z' }], cursor: 'next' });
assert.deepEqual(seen[0], { prefix: 'docs/', cursor: 'opaque', limit: 100 });
const object = await handleRequest(req('/v1/object?key=docs%2Fa.txt'), env);
assert.equal(object.status, 200);
assert.equal(object.headers.get('content-disposition'), 'attachment');
assert.equal(object.headers.get('x-content-type-options'), 'nosniff');
assert.equal(await object.text(), 'abc');
assert.equal((await handleRequest(req('/v1/object?key=none'), env)).status, 404);
assert.equal((await handleRequest(req('/v1/object?key=%00'), env)).status, 400);
const failed = await handleRequest(req('/v1/objects'), { ...env, R2: { async list() { throw Error('secret details'); } } });
assert.equal(failed.status, 502);
assert.ok(!(await failed.text()).includes('secret details'));
console.log('Experimental Cloudflare-native R2 reader tests passed');

// Write access is explicitly opt-in, bounded, and never overwrites an existing key.
const stored = new Map();
const writes = { ...env, KOVA_NATIVE_WRITES: 'enabled', R2: {
  async put(key, bytes, options) {
    assert.equal(options.onlyIf.get('if-none-match'), '*');
    if (stored.has(key)) return null;
    stored.set(key, new Uint8Array(bytes));
    return { key };
  },
} };
const upload = (body, length, token = secret) => new Request('https://native.test/v1/object?key=upload.txt', {
  method: 'PUT', headers: { authorization: `Bearer ${token}`, ...(length === null ? {} : { 'content-length': String(length) }) }, body,
});
assert.equal((await handleRequest(upload('abc', 3), env)).status, 403);
assert.equal((await handleRequest(upload('abc', 3, 'wrong'), writes)).status, 401);
assert.equal((await handleRequest(upload('abc', null), writes)).status, 411);
assert.equal((await handleRequest(upload('abc', 8388609), writes)).status, 413);
assert.equal((await handleRequest(upload('abc', 2), writes)).status, 413);
assert.equal((await handleRequest(upload('abc', 4), writes)).status, 400);
assert.equal(stored.size, 0);
assert.equal((await handleRequest(upload('abc', 3), writes)).status, 201);
assert.equal((await handleRequest(upload('xyz', 3), writes)).status, 409);
assert.deepEqual([...stored.get('upload.txt')], [97, 98, 99]);
console.log('Experimental opt-in create-only upload tests passed');
