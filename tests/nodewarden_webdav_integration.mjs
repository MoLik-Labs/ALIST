// Optional integration against a separately bundled, pinned NodeWarden uploader.
// NODEWARDEN_UPLOADER must be an absolute path; no upstream code is vendored.
import assert from 'node:assert/strict';
import { spawn } from 'node:child_process';
import { mkdtemp, mkdir, rm } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { pathToFileURL } from 'node:url';
import net from 'node:net';

const bundle = process.env.NODEWARDEN_UPLOADER;
if (!bundle || !path.isAbsolute(bundle)) throw new Error('Set NODEWARDEN_UPLOADER to the absolute path of a pinned bundled uploader');
const { createRemoteBackupTransferSession } = await import(pathToFileURL(bundle));
const temp = await mkdtemp(path.join(os.tmpdir(), 'kova-nodewarden-test-'));
const storage = path.join(temp, 'storage'); await mkdir(storage);
const server = net.createServer();
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
const port = server.address().port;
await new Promise(resolve => server.close(resolve));
const endpoint = `http://127.0.0.1:${port}`;
const proc = spawn(path.resolve('alist'), ['server', '--data', path.join(temp, 'data'), '--port', String(port)], {
  env: { ...process.env, ALIST_ADDR: '127.0.0.1', ALIST_ADMIN_PASSWORD: 'IntegrationPassword123!' }, stdio: 'ignore'
});
try {
  let ready = false;
  for (let i = 0; i < 100 && !ready; i++) {
    if (proc.exitCode !== null) throw new Error('Kova failed to start');
    try { ready = (await (await fetch(endpoint + '/ping')).text()) === 'pong'; } catch { await new Promise(r => setTimeout(r, 100)); }
  }
  assert.ok(ready, 'Kova did not start');
  const post = async (route, body, token) => {
    const response = await fetch(endpoint + route, { method: 'POST', headers: { 'content-type': 'application/json', ...(token ? { authorization: token } : {}) }, body: JSON.stringify(body) });
    const value = await response.json(); assert.equal(value.code, 200, value.message); return value.data;
  };
  const login = await post('/api/auth/login', { username: 'admin', password: 'IntegrationPassword123!' });
  const mount = await post('/api/admin/storage/create', { mount_path: '/backup', driver: 'Local', addition: { root_folder_path: storage } }, login.token);
  assert.equal(mount.warning, '');
  // NodeWarden rejects loopback URLs by design. Rewrite only this fixed test
  // hostname to loopback; this does not test DNS, TLS or a deployed Worker.
  const realFetch = globalThis.fetch;
  globalThis.fetch = (input, init) => {
    const url = new URL(input);
    assert.equal(url.origin, 'https://nodewarden-fixture.example.com');
    return realFetch(endpoint + url.pathname + url.search, init);
  };
  const session = createRemoteBackupTransferSession({ type: 'webdav', destination: { baseUrl: 'https://nodewarden-fixture.example.com/dav/backup', username: 'admin', password: 'IntegrationPassword123!', remotePath: 'nodewarden' } });
  const sample = new TextEncoder().encode('dummy backup only');
  await session.uploadArchive(sample, 'test.zip');
  await session.putFile('attachments/item.bin', sample);
  assert.equal(await session.exists('test.zip'), true);
  assert.equal((await session.stat('test.zip')).size, sample.length);
  const listing = await session.list('');
  assert.ok(listing.items.some(x => x.name === 'test.zip'));
  assert.ok(listing.items.some(x => x.name === 'attachments' && x.isDirectory));
  assert.deepEqual((await session.download('test.zip')).bytes, sample);
  await session.deleteFile('test.zip');
  assert.equal(await session.exists('test.zip'), false);
  globalThis.fetch = realFetch;
  console.log('Pinned NodeWarden uploader WebDAV flow passed locally with synthetic data');
} finally {
  if (proc.exitCode === null) { proc.kill('SIGTERM'); await new Promise(resolve => proc.once('exit', resolve)); }
  await rm(temp, { recursive: true, force: true });
}
