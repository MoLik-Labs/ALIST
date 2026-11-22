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
console.log('Local workerd and emulated R2 smoke test passed');
