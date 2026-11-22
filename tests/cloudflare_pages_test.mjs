import assert from 'node:assert/strict';
import { onRequest } from '../deploy/cloudflare-pages/functions/_middleware.js';

const original = globalThis.fetch;
let target;
try {
  globalThis.fetch = async (url, init) => {
    target = new URL(url);
    assert.equal(init.redirect, 'manual');
    return new Response('upstream', { headers: { 'cache-control': 'public, max-age=600' } });
  };
  const request = new Request('https://example.pages.dev//attacker.example/p?x=1');
  const response = await onRequest({ request, env: { KOVA_ORIGIN: 'https://origin.example' } });
  assert.equal(response.status, 200);
  assert.equal(target.origin, 'https://origin.example');
  assert.equal(target.pathname, '//attacker.example/p');
  assert.equal(response.headers.get('cache-control'), 'private, no-store');
  assert.equal(await response.text(), 'upstream');
  const missing = await onRequest({ request, env: {} });
  assert.equal(missing.status, 503);
} finally {
  globalThis.fetch = original;
}
console.log('Pages middleware proxy adapter passed');
