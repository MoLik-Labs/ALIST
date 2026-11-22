// Only for a Wrangler server using wrangler.local.toml and the documented local test token.
import assert from 'node:assert/strict';
const base = 'http://127.0.0.1:8787';
const headers = { Authorization: 'Bearer local-test-secret-at-least-32-characters' };
let response = await fetch(`${base}/ping`);
assert.equal(response.status, 200);
assert.equal((await response.json()).experimental, true);
response = await fetch(`${base}/v1/objects`);
assert.equal(response.status, 401);
response = await fetch(`${base}/v1/objects`, { headers });
assert.equal(response.status, 200);
assert.ok(Array.isArray((await response.json()).objects));
response = await fetch(`${base}/v1/object?key=__kova_runtime_smoke_missing__`, { headers });
assert.equal(response.status, 404);
if (process.env.TEST_LOCAL_WRITES === '1') {
  // Random key avoids conflicts with prior local runs. This leaves a test object
  // in the emulated bucket; never point this test at a remote service.
  const key = `local-smoke-${crypto.randomUUID()}`;
  const url = `${base}/v1/object?key=${encodeURIComponent(key)}`;
  response = await fetch(url, { method: 'PUT', headers, body: 'first' });
  assert.equal(response.status, 201);
  response = await fetch(url, { method: 'PUT', headers, body: 'other' });
  assert.equal(response.status, 409);
  response = await fetch(url, { headers });
  assert.equal(await response.text(), 'first');
}
console.log('Local workerd and emulated R2 smoke test passed');
