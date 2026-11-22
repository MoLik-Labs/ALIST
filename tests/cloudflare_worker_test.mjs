import assert from 'node:assert/strict';
import { handleRequest } from '../deploy/cloudflare-worker/src/index.js';

const incoming = new Request('https://files.example.com/api/fs/put?mode=stream', {
  method: 'PUT',
  headers: {
    Authorization: 'Bearer client-token',
    'Content-Type': 'application/octet-stream',
    'Content-Length': '11',
    'CF-Connecting-IP': '203.0.113.8',
    'X-Forwarded-For': '198.51.100.200',
    'X-Forwarded-Host': 'attacker.invalid',
    'X-Forwarded-Proto': 'http',
    Connection: 'keep-alive, x-hop-private',
    'X-Hop-Private': 'drop-me',
  },
  body: 'hello world',
});

let forwardedBody = '';
let forwardedUrl;
let forwardedInit;
const upstream = await handleRequest(incoming, { KOVA_ORIGIN: 'https://kova-origin.example' }, async (url, init) => {
  forwardedUrl = new URL(url);
  forwardedInit = init;
  forwardedBody = await new Response(init.body).text();
  return new Response('streamed-from-origin', {
    status: 206,
    headers: {
      'Content-Type': 'text/plain',
      'Content-Range': 'bytes 0-19/20',
      'Connection': 'x-origin-hop',
      'X-Origin-Hop': 'drop-me-too',
      'Cache-Control': 'public, max-age=3600',
    },
  });
});

assert.equal(forwardedUrl.href, 'https://kova-origin.example/api/fs/put?mode=stream');
assert.equal(forwardedInit.method, 'PUT');
assert.equal(forwardedBody, 'hello world');
const requestHeaders = forwardedInit.headers;
assert.equal(requestHeaders.get('authorization'), 'Bearer client-token');
assert.equal(requestHeaders.get('x-forwarded-for'), '203.0.113.8', 'untrusted forwarding headers must be overwritten');
assert.equal(requestHeaders.get('x-forwarded-host'), 'files.example.com');
assert.equal(requestHeaders.get('x-forwarded-proto'), 'https');
assert.equal(requestHeaders.get('host'), null);
assert.equal(requestHeaders.get('x-hop-private'), null, 'Connection-nominated headers must be removed');
assert.equal(requestHeaders.get('x-forwarded-host').includes('attacker'), false);
assert.equal(forwardedInit.redirect, 'manual', 'upstream redirects must not be followed inside the Worker');
assert.equal(forwardedInit.signal, incoming.signal, 'client disconnects should cancel the upstream fetch');
assert.equal(forwardedInit.cf.cacheTtl, 0);
assert.equal(upstream.status, 206);
assert.equal(upstream.headers.get('content-range'), 'bytes 0-19/20');
assert.equal(upstream.headers.get('x-origin-hop'), null);
assert.equal(upstream.headers.get('cache-control'), 'private, no-store');
assert.equal(await upstream.text(), 'streamed-from-origin');

// A network-path reference must remain an origin path, never become a host.
for (const path of ['//attacker.invalid/collect?x=1', '///attacker.invalid/p', '/%2F%2Fattacker.invalid/x', '/file%20name?x=%2F']) {
  const req = new Request('https://files.example.com' + path, { headers: { Authorization: 'Bearer sensitive-token' } });
  let target, sentHeaders;
  const result = await handleRequest(req, { KOVA_ORIGIN: 'https://kova-origin.example' }, async (url, init) => {
    target = new URL(url);
    sentHeaders = init.headers;
    return new Response('ok');
  });
  const incomingUrl = new URL(req.url);
  assert.equal(result.status, 200);
  assert.equal(target.origin, 'https://kova-origin.example', 'request path must not replace the configured host');
  assert.equal(target.pathname, incomingUrl.pathname);
  assert.equal(target.search, incomingUrl.search);
  assert.equal(sentHeaders.get('authorization'), 'Bearer sensitive-token');
}

const headRequest = new Request('https://files.example.com/healthz', { method: 'HEAD' });
let headInit;
const headResponse = await handleRequest(headRequest, { KOVA_ORIGIN: 'https://kova-origin.example' }, async (_url, init) => {
  headInit = init;
  return new Response(null, { status: 200 });
});
assert.equal(headInit.body, undefined, 'GET/HEAD requests must not fabricate an upload stream');
assert.equal(headResponse.status, 200);

const redirect = await handleRequest(new Request('https://files.example.com/old'), {
  KOVA_ORIGIN: 'https://kova-origin.example',
}, async () => new Response(null, {
  status: 302,
  headers: { Location: 'http://kova-origin.example/new?from=origin' },
}));
assert.equal(redirect.headers.get('location'), 'https://files.example.com/new?from=origin', 'same-origin redirects should stay on the edge hostname');
assert.equal(redirect.headers.get('cache-control'), 'private, no-store');

const externalRedirect = await handleRequest(new Request('https://files.example.com/out'), {
  KOVA_ORIGIN: 'https://kova-origin.example',
}, async () => new Response(null, {
  status: 302,
  headers: { Location: 'https://downloads.example.net/file.zip' },
}));
assert.equal(externalRedirect.headers.get('location'), 'https://downloads.example.net/file.zip', 'external redirects must remain unchanged');

for (const invalidOrigin of ['', 'http://kova-origin.example', 'https://user:pass@kova-origin.example', 'https://kova-origin.example/base']) {
  const notConfigured = await handleRequest(new Request('https://files.example.com/'), { KOVA_ORIGIN: invalidOrigin }, async () => {
    assert.fail('invalid origin must be rejected before an upstream fetch');
  });
  assert.equal(notConfigured.status, 503);
  assert.equal(notConfigured.headers.get('cache-control'), 'no-store');
}

const failed = await handleRequest(new Request('https://files.example.com/'), { KOVA_ORIGIN: 'https://kova-origin.example' }, async () => {
  throw new Error('private origin details must not escape to the client');
});
assert.equal(failed.status, 502);
assert.equal(await failed.text(), 'Kova origin is unavailable. Check the configured KOVA_ORIGIN and origin health.');

console.log('Cloudflare Worker proxy smoke passed: streamed requests/responses, trusted forwarding headers, hop-by-hop stripping, no edge caching, origin validation, and generic upstream errors.');
