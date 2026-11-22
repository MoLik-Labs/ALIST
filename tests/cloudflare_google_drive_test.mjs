import assert from 'node:assert/strict';
import { handleRequest } from '../deploy/cloudflare-native/src/index.js';

const env = { KOVA_NATIVE_TOKEN: 'test-only-secret-at-least-32-characters', GOOGLE_CLIENT_ID: 'client-id', GOOGLE_CLIENT_SECRET: 'client-secret', GOOGLE_REFRESH_TOKEN: 'refresh-secret' };
const makeRequest = (path, token = env.KOVA_NATIVE_TOKEN, method = 'GET', extraHeaders = {}) => new Request(`https://worker.test${path}`, { method, headers: { Authorization: `Bearer ${token}`, ...extraHeaders } });
const originalFetch = globalThis.fetch;
const requests = [];
let mode = 'list';
globalThis.fetch = async (url, options) => {
  requests.push({ url: String(url), options });
  if (String(url) === 'https://oauth2.googleapis.com/token') {
    if (mode === 'auth-fail') return new Response('refresh-secret', { status: 401 });
    return Response.json({ access_token: 'access-secret' });
  }
  if (mode === 'redirect') return new Response(null, { status: 302, headers: { location: 'https://evil.test/steal' } });
  if (mode === 'download') return new Response('hello', { headers: { 'content-type': 'text/html', 'set-cookie': 'bad=1' } });
  if (mode === 'partial') return new Response('part', { status: 206 });
  if (mode === 'range') return new Response('llo', { status: 206, headers: { 'content-range': 'bytes 2-4/5', 'content-length': '3', 'set-cookie': 'bad=1' } });
  if (mode === 'suffix') return new Response('lo', { status: 206, headers: { 'content-range': 'bytes 3-4/5', 'content-length': '2' } });
  if (mode === 'range-invalid') return new Response('llo', { status: 206, headers: { 'content-range': 'bytes 1-3/5', 'content-length': '3' } });
  if (mode === 'range-length-mismatch') return new Response('llo', { status: 206, headers: { 'content-range': 'bytes 2-4/5', 'content-length': '4' } });
  if (mode === 'range-unsatisfiable') return new Response(null, { status: 416, headers: { 'content-range': 'bytes */5' } });
  if (mode === 'oversize') return new Response('a'.repeat(1024 * 1024 + 1));
  return Response.json({ nextPageToken: 'next', files: [{ id: 'file-1', name: 'File', mimeType: 'text/plain', extra: 'omit' }], secret: 'omit' });
};
try {
  assert.equal((await handleRequest(makeRequest('/v1/google-drive/files', 'bad'), env)).status, 401);
  assert.equal((await handleRequest(makeRequest('/v1/google-drive/files', env.KOVA_NATIVE_TOKEN, 'PUT'), env)).status, 405);
  assert.equal((await handleRequest(makeRequest('/v1/google-drive/files?parent=a%27%20or%20true'), env)).status, 400);
  assert.equal((await handleRequest(makeRequest('/v1/google-drive/files'), { KOVA_NATIVE_TOKEN: env.KOVA_NATIVE_TOKEN })).status, 503);
  assert.equal(requests.length, 0);
  let response = await handleRequest(makeRequest('/v1/google-drive/files?parent=folder_1&cursor=opaque%2Bnext'), env);
  assert.equal(response.status, 200);
  assert.deepEqual(await response.json(), { files: [{ id: 'file-1', name: 'File', mimeType: 'text/plain' }], nextPageToken: 'next' });
  assert.equal(requests[0].url, 'https://oauth2.googleapis.com/token');
  assert.equal(requests[0].options.redirect, 'manual');
  assert.equal(requests[0].options.body.get('refresh_token'), 'refresh-secret');
  const target = new URL(requests[1].url);
  assert.equal(target.origin, 'https://www.googleapis.com');
  assert.equal(target.searchParams.get('q'), "'folder_1' in parents and trashed = false");
  assert.equal(target.searchParams.get('pageToken'), 'opaque+next');
  assert.equal(requests[1].options.headers.authorization, 'Bearer access-secret');
  assert.equal(requests[1].options.redirect, 'manual');
  mode = 'download';
  // The header deadline must be cleared once streaming can begin; otherwise
  // an otherwise healthy download is aborted after 30 seconds.
  const realSetTimeout = globalThis.setTimeout, realClearTimeout = globalThis.clearTimeout;
  let pendingTimer, cleared = false;
  globalThis.setTimeout = (fn, delay) => { assert.equal(delay, 30000); pendingTimer = { fn }; return pendingTimer; };
  globalThis.clearTimeout = timer => { assert.equal(timer, pendingTimer); cleared = true; };
  try { response = await handleRequest(makeRequest('/v1/google-drive/file?id=file-1'), env); }
  finally { globalThis.setTimeout = realSetTimeout; globalThis.clearTimeout = realClearTimeout; }
  assert.ok(cleared, 'download header deadline must be cleared');
  assert.equal(requests.at(-1).options.signal.aborted, false);
  assert.equal(response.status, 200);
  assert.equal(response.headers.get('content-type'), 'application/octet-stream');
  assert.equal(response.headers.get('set-cookie'), null);
  assert.equal(await response.text(), 'hello');
  const downloadWithRange = range => makeRequest('/v1/google-drive/file?id=file-1', env.KOVA_NATIVE_TOKEN, 'GET', { Range: range });
  const beforeInvalidRanges = requests.length;
  for (const invalid of ['bytes=2-1', 'bytes=-0', 'bytes=0-1,3-4', 'bytes=9223372036854775808-', 'items=1-2', 'bytes=-']) {
    assert.equal((await handleRequest(downloadWithRange(invalid), env)).status, 400, invalid);
  }
  assert.equal(requests.length, beforeInvalidRanges, 'invalid ranges must not contact Google');
  mode = 'range';
  response = await handleRequest(downloadWithRange('bytes=2-4'), env);
  assert.equal(response.status, 206);
  assert.equal(response.headers.get('content-range'), 'bytes 2-4/5');
  assert.equal(response.headers.get('content-length'), '3');
  assert.equal(response.headers.get('set-cookie'), null);
  assert.equal(await response.text(), 'llo');
  assert.equal(requests.at(-1).options.headers.range, 'bytes=2-4');
  assert.equal((await handleRequest(downloadWithRange('bytes=2-'), env)).status, 206);
  mode = 'suffix';
  assert.equal((await handleRequest(downloadWithRange('bytes=-2'), env)).status, 206);
  mode = 'range-invalid';
  assert.equal((await handleRequest(downloadWithRange('bytes=2-4'), env)).status, 502);
  mode = 'range-length-mismatch';
  assert.equal((await handleRequest(downloadWithRange('bytes=2-4'), env)).status, 502);
  mode = 'range-unsatisfiable';
  response = await handleRequest(downloadWithRange('bytes=9-'), env);
  assert.equal(response.status, 416);
  assert.equal(response.headers.get('content-range'), 'bytes */5');
  mode = 'download';
  response = await handleRequest(downloadWithRange('bytes=2-4'), env);
  assert.equal(response.status, 200, 'upstream may legitimately ignore a range');
  assert.equal(await response.text(), 'hello');
  mode = 'partial';
  assert.equal((await handleRequest(makeRequest('/v1/google-drive/file?id=file-1'), env)).status, 502);
  mode = 'redirect';
  response = await handleRequest(makeRequest('/v1/google-drive/file?id=file-1'), env);
  assert.equal(response.status, 502);
  assert.equal(response.headers.get('location'), null);
  mode = 'auth-fail';
  response = await handleRequest(makeRequest('/v1/google-drive/files'), env);
  assert.equal(response.status, 502);
  assert.ok(!(await response.text()).includes('refresh-secret'));
  mode = 'oversize';
  assert.equal((await handleRequest(makeRequest('/v1/google-drive/files'), env)).status, 502);
  console.log('Experimental Google Drive connector tests passed');
} finally { globalThis.fetch = originalFetch; }
