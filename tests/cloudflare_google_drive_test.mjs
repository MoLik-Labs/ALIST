import assert from 'node:assert/strict';
import { handleRequest } from '../deploy/cloudflare-native/src/index.js';

const env = { KOVA_NATIVE_TOKEN: 'test-only-secret-at-least-32-characters', GOOGLE_CLIENT_ID: 'client-id', GOOGLE_CLIENT_SECRET: 'client-secret', GOOGLE_REFRESH_TOKEN: 'refresh-secret' };
const makeRequest = (path, token = env.KOVA_NATIVE_TOKEN, method = 'GET') => new Request(`https://worker.test${path}`, { method, headers: { Authorization: `Bearer ${token}` } });
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
  response = await handleRequest(makeRequest('/v1/google-drive/file?id=file-1'), env);
  assert.equal(response.status, 200);
  assert.equal(response.headers.get('content-type'), 'application/octet-stream');
  assert.equal(response.headers.get('set-cookie'), null);
  assert.equal(await response.text(), 'hello');
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
