import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { Window } from 'happy-dom';

const window = new Window({ url: 'http://localhost:5244/' });
window.document.write(await readFile('internal/goserver/ui/index.html', 'utf8'));
const calls = [];
let caps = { create: true, delete: true, move: true, share: true };
let delayedShare = null;
let shares = [];
window.fetch = async (input, options = {}) => {
  const url = new URL(input, window.location.href);
  calls.push({ path: url.searchParams.get('path'), method: options.method || 'GET', headers: options.headers, url: url.toString() });
  if (options.headers?.Authorization !== `Bearer ${'x'.repeat(32)}`) return { ok: false, status: 401 };
  if (url.pathname === '/api/local/shares' && options.method === 'POST') {
    if (delayedShare) await delayedShare;
    const share = { url: '/s/' + 'a'.repeat(43), expires: 1900000000, path: '<img src=x onerror=alert(1)>.txt' };
    shares.push(share);
    return { ok: true, status: 201, json: async () => share };
  }
  if (url.pathname === '/api/local/shares' && !options.method) return { ok: true, status: 200, json: async () => ({ shares }) };
  if (url.pathname.startsWith('/api/local/shares/') && options.method === 'DELETE') { shares = shares.filter(item => item.url !== '/s/' + url.pathname.split('/').at(-1)); return { ok: true, status: 204 }; }
  return { ok: true, status: 200, json: async () => ({ capabilities: caps, entries: url.searchParams.get('path') === '/' ? [
    { name: 'Documents', is_dir: true, size: 0, modified: 1700000000 },
    { name: '<img src=x onerror=alert(1)>.txt', is_dir: false, size: 5, modified: 1700000000 }
  ] : [] }) };
};
window.eval(await readFile('internal/goserver/ui/app.js', 'utf8'));
const document = window.document;
const tick = () => new Promise(resolve => setTimeout(resolve, 10));
async function eventually(fn) { for (let i = 0; i < 100; i++) { if (fn()) return; await tick(); } assert.fail('timed out waiting for UI'); }
document.querySelector('#token').value = 'x'.repeat(32);
document.querySelector('#login-form').dispatchEvent(new window.Event('submit', { bubbles: true, cancelable: true }));
await eventually(() => !document.querySelector('#workspace').hidden);
assert.equal(document.querySelector('#token').value, '');
assert.equal(document.querySelector('#file-list').querySelectorAll('img').length, 0, 'filenames must not be interpreted as HTML');
assert.match(document.querySelector('#file-list').textContent, /<img src=x/);
assert.equal(calls[0].headers.Authorization, `Bearer ${'x'.repeat(32)}`);
assert.ok(!calls[0].url.includes('x'.repeat(32)), 'token must not appear in URL');
assert.equal(document.querySelectorAll('.share-action').length, 1, 'only files can be shared');
document.querySelector('.share-action').click();
await eventually(() => !document.querySelector('#share-result').hidden);
assert.equal(document.querySelector('#share-url').value, `http://localhost:5244/s/${'a'.repeat(43)}`);
document.querySelector('#share-close').click();
document.querySelector('#share-manage').click();
await eventually(() => document.querySelector('.share-list-view'));
assert.equal(document.querySelector('#share-list').querySelectorAll('img').length, 0, 'shared file paths must not be HTML');
assert.match(document.querySelector('#share-list').textContent, /<img src=x/);
document.querySelector('.share-list-view').click();
assert.equal(document.querySelector('#share-url').value, `http://localhost:5244/s/${'a'.repeat(43)}`);
document.querySelector('#share-revoke').click();
await eventually(() => calls.some(call => call.method === 'DELETE' && new URL(call.url).pathname === `/api/local/shares/${'a'.repeat(43)}`));
await eventually(() => document.querySelector('#share-result').hidden);
assert.equal(document.querySelector('#share-url').value, '');
await eventually(() => document.querySelector('#share-list').textContent.includes('Chưa có'));
window.confirm = () => false;
document.querySelector('.delete-action').click();
await tick();
assert.equal(calls.some(call => call.method === 'DELETE' && new URL(call.url).pathname === '/api/local/file'), false, 'cancel must not send a file delete');
window.prompt = () => 'Renamed';
document.querySelector('.rename-action').click();
await eventually(() => calls.some(call => call.method === 'POST' && call.path === '/Documents' && new URL(call.url).searchParams.get('destination') === '/Renamed'));
document.querySelector('.file-name').click();
await eventually(() => document.querySelector('#current-path').textContent === '/Documents');
assert.equal(calls.at(-1).path, '/Documents');
window.prompt = () => 'New folder';
document.querySelector('#mkdir').click();
await eventually(() => calls.some(call => call.method === 'POST' && call.path === '/Documents/New folder'));
document.querySelector('#breadcrumbs button').click();
await eventually(() => document.querySelector('.share-action'));
let finishShare;
delayedShare = new Promise(resolve => { finishShare = resolve; });
document.querySelector('.share-action').click();
document.querySelector('#logout').click();
finishShare();
await tick();
assert.equal(document.querySelector('#share-result').hidden, true, 'a late share response must not reveal a link after logout');
assert.equal(document.querySelector('#share-url').value, '');
delayedShare = null;
assert.equal(document.querySelector('#workspace').hidden, true);
assert.equal(document.querySelector('#login').hidden, false);
assert.equal(window.localStorage.length, 0, 'token must not be persisted');
caps = { create: false, delete: false, move: false, share: false };
document.querySelector('#token').value = 'x'.repeat(32);
document.querySelector('#login-form').dispatchEvent(new window.Event('submit', { bubbles: true, cancelable: true }));
await eventually(() => !document.querySelector('#workspace').hidden);
assert.equal(document.querySelector('#mkdir').hidden, true);
assert.equal(document.querySelector('#upload-control').hidden, true);
assert.equal(document.querySelector('.delete-action'), null);
assert.equal(document.querySelector('.rename-action'), null);
assert.equal(document.querySelector('.share-action'), null);
assert.equal(document.querySelector('#share-manage').hidden, true);
window.close();
console.log('Go workspace UI smoke test passed');
