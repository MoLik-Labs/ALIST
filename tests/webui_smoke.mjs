import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { Window } from 'happy-dom';

const embeddedHeader = await readFile(new URL('../include/alist/web_ui.hpp', import.meta.url), 'utf8');
const embedded = embeddedHeader.match(/kEmbeddedWebUiHtml = R"([A-Z_]+)\(([\s\S]*)\)\1";/);
assert.ok(embedded, 'generated C++ header must contain the embedded Workspace App');
const embeddedHtml = embedded[2];
const inlineScript = embeddedHtml.match(/<script>([\s\S]*?)<\/script>/i);
assert.ok(inlineScript, 'embedded application must contain its inline runtime script');
const app = inlineScript[1];
const html = embeddedHtml.replace(inlineScript[0], '');
const wait = ms => new Promise(resolve => setTimeout(resolve, ms));
async function eventually(predicate, message, timeout = 2000) {
  const end = Date.now() + timeout;
  while (Date.now() < end) {
    if (predicate()) return;
    await wait(10);
  }
  assert.fail(message);
}

function createHarness({ path = '/', role = 2, items = [], backendDown = false } = {}) {
  const window = new Window({ url: `http://kova.test${path}` });
  let collatorCreations = 0;
  const OriginalCollator = window.Intl.Collator;
  window.Intl.Collator = function (...args) { collatorCreations++; return new OriginalCollator(...args); };
  window.document.write(html);
  window.document.close();
  window.localStorage.setItem('kova.session.v1', 'smoke-token');
  const calls = [];
  const files = items.length ? items : [{ name: 'hello.txt', size: 12, is_dir: false, modified: '2026-11-16T00:00:00Z', type: 5 }];
  const user = role === 2
    ? { id: 1, username: 'admin', base_path: '/', role: 2, role_ids: [2], permission: 131071, otp: false }
    : { id: 2, username: 'guest', base_path: '/', role: 1, role_ids: [], permission: 1, otp: false };
  const drivers = [
    { name: 'Local', display_name: 'Local filesystem', fields: [
      { name: 'root_folder_path', label: 'Root folder', type: 'string', default_val: '', required: true },
      { name: 'show_hidden', label: 'Show hidden files', type: 'bool', default_val: false },
    ] },
    { name: 'Catbox', display_name: 'Catbox public links (read-only)', fields: [{ name: 'files', label: 'Public Catbox links', type: 'text', required: true }] },
    { name: 'S3', display_name: 'S3 compatible', fields: [
      { name: 'bucket', label: 'Bucket', type: 'string', default_val: '' },
      { name: 'region', label: 'Region', type: 'string', default_val: 'us-east-1' },
    ] },
  ];
  const adminStats = {
    version: 'v4.2.0', engine: 'Kova Native C++20', uptime_sec: 60, rss_bytes: 4096,
    peak_rss_bytes: 8192, threads: 4, cpu_cores: 8, active_connections: 1,
    total_requests: 20, bytes_sent: 1000, bytes_received: 2000, vfs_cache_hits: 3,
    vfs_cache_misses: 4, vfs_cache_entries: 5, mounted_storages: 1, db_size_bytes: 2048,
    fts_indexed_files: 10, tasks_undone: 0, tasks_done: 0,
  };
  const roles = [{ id: 2, name: 'Administrators', description: 'Full access', permission_scopes: [{ path: '/', permission: 131071 }] }];
  const response = (data, { status = 200, contentType = 'application/json', text = 'sample text\n', blobType = 'text/plain' } = {}) => ({
    ok: status >= 200 && status < 300,
    status,
    statusText: status === 200 ? 'OK' : 'Error',
    headers: { get: name => name.toLowerCase() === 'content-type' ? contentType : null },
    async json() { return { code: status >= 200 && status < 300 ? 200 : status, message: 'success', data }; },
    async text() { return text; },
    async blob() { return new window.Blob([text], { type: blobType }); },
  });

  window.fetch = async (input, options = {}) => {
    const url = new URL(String(input), window.location.href);
    let body = options.body;
    if (typeof body === 'string') {
      try { body = JSON.parse(body); } catch { /* plain upload body */ }
    }
    calls.push({ path: url.pathname, query: url.searchParams, method: options.method || 'GET', body, headers: options.headers || {} });
    const pathName = url.pathname;
    if (backendDown) throw new Error('network unavailable');
    if (pathName === '/ping') return response(null, { contentType: 'text/plain', text: 'pong' });
    if (pathName === '/api/public/settings') return response({ app_name: 'Kova', site_title: 'Kova', version: 'v4.2.0', guest_enabled: true });
    if (pathName === '/api/me') return response(user);
    if (pathName === '/api/fs/list') return response({ content: files.map(item => ({ ...item })), total: files.length, write: role === 2, provider: 'Local', readme: '# Workspace' });
    if (pathName === '/api/fs/dirs') return response([{ name: 'Documents', is_dir: true, type: 1 }, { name: '..', is_dir: true, type: 1 }]);
    if (pathName === '/api/fs/get') {
      const reqPath = body?.path || '/hello.txt';
      return response({ name: reqPath.split('/').at(-1), type: 5, is_dir: false, size: 12, raw_url: `/d${reqPath}` });
    }
    if (pathName === '/api/fs/search') return response({ content: [], total: 0 });
    if (pathName === '/api/admin/system/stats') return response(adminStats);
    if (pathName === '/api/admin/system/uptime') return response({ state: 'up', successful: 23, samples: 25, checkedAt: Date.now() });
    if (pathName === '/api/admin/storage/list') return response({ content: [{ id: 1, mount_path: '/data', driver: 'Local', status: 'work', remark: 'primary' }], total: 1 });
    if (pathName === '/api/admin/driver/list') return response(drivers);
    if (pathName === '/api/admin/storage/get') return response({ id: 1, mount_path: '/data', driver: 'Local', addition: '{"root_folder_path":"/tmp"}', order: 0, cache_expiration: 30, remark: 'primary', order_direction: 'ASC' });
    if (pathName === '/api/admin/user/list') return response({ content: [{ id: 1, username: 'admin', base_path: '/', role: 2, role_ids: [2], disabled: false, otp: false }], total: 1 });
    if (pathName === '/api/admin/role/list') return response({ content: roles, total: roles.length });
    if (pathName === '/api/admin/meta/list') return response({ content: [{ id: 1, path: '/private', hide: '*.secret', password: '********', write: true }], total: 1 });
    if (pathName === '/api/admin/setting/list') return response({ site_title: 'Kova', default_page_size: '50', pagination_type: 'all', enable_search: 'true' });
    if (pathName === '/api/admin/index/progress') return response({ obj_count: 10, is_done: true });
    if (pathName === '/api/share/list') return response({ content: [], total: 0 });
    if (pathName === '/api/task/undone' || pathName === '/api/task/done') return response([]);
    if (pathName === '/api/public/share/info') return response({ share_id: 'share-123', name: 'Shared docs', is_dir: true, size: 0, type: 1, modified: 1794787200, creator: 'owner', has_password: true, views: 6 });
    if (pathName === '/api/public/share/auth') return response({ token: 'share-token' });
    if (pathName === '/api/public/share/list') return response({ content: [
      { name: 'readme.md', size: 32, is_dir: false, type: 5, modified: '2026-11-15T12:00:00Z' },
      { name: '<img src=x onerror=alert(1)>.txt', size: 10, is_dir: false, type: 5, modified: '2026-11-15T12:00:00Z' },
      { name: 'Nested', size: 0, is_dir: true, type: 1, modified: '2026-11-15T12:00:00Z' },
    ], total: 3 });
    if (pathName === '/api/auth/login') return response({ token: 'new-token' });
    if (pathName === '/api/admin/setting/reset_token') return response({ token: 'one-time-system-token' });
    if (pathName.startsWith('/api/')) return response({});
    return response(null, { contentType: 'text/plain', text: 'const sample = "safe";\n', blobType: 'text/plain' });
  };

  class FakeXHR {
    constructor() { this.headers = {}; this.upload = { addEventListener() {} }; this.status = 0; this.responseText = ''; }
    open(method, url) { this.method = method; this.url = url; }
    setRequestHeader(name, value) { this.headers[name] = value; }
    send(body) {
      calls.push({ path: this.url, method: this.method, headers: this.headers, body });
      this.status = 200;
      this.responseText = JSON.stringify({ code: 200, message: 'success', data: null });
      queueMicrotask(() => this.onload?.());
    }
    abort() { this.onabort?.(); }
  }
  window.XMLHttpRequest = FakeXHR;
  window.eval(app);
  return { window, calls, collatorCount: () => collatorCreations };
}

async function testExplorerAndAdmin() {
  const items = Array.from({ length: 10000 }, (_, i) => ({
    name: `file-${String(i).padStart(5, '0')}.txt`, size: i + 1, is_dir: false,
    modified: '2026-11-16T00:00:00Z', type: 5,
  }));
  const { window, calls, collatorCount } = createHarness({ items });
  const { document } = window;
  await eventually(() => document.querySelectorAll('#explorerContent tbody tr').length === 160, 'Explorer did not render the initial virtualized window');
  assert.ok(document.querySelector('[data-act="load-more"]'), 'large listings must expose progressive loading');
  assert.equal(document.querySelectorAll('#sidebarContent [aria-current="page"]').length, 1, 'only one current navigation item');
  window.innerWidth = 600;
  window.dispatchEvent(new window.Event('resize'));
  assert.ok(document.getElementById('appSidebar').hasAttribute('inert'), 'closed mobile navigation is not keyboard reachable');
  document.getElementById('btnMobileNav').click();
  assert.ok(!document.getElementById('appSidebar').hasAttribute('inert'));
  document.getElementById('btnMobileNav').dispatchEvent(new window.KeyboardEvent('keydown', { key: 'Escape', bubbles: true, cancelable: true }));
  assert.ok(document.getElementById('appSidebar').hasAttribute('inert'));
  assert.equal(document.activeElement, document.getElementById('btnMobileNav'));
  window.innerWidth = 1200;
  window.dispatchEvent(new window.Event('resize'));
  assert.ok(!document.getElementById('appSidebar').hasAttribute('inert'), 'desktop navigation remains available');
  const initialSorts = collatorCount();
  document.querySelector('[data-act="load-more"]').click();
  await eventually(() => document.querySelectorAll('#explorerContent tbody tr').length === 320, 'Load more did not expand the rendered window');
  assert.equal(collatorCount(), initialSorts, 'progressive loading must reuse sorted results');
  document.getElementById('btnViewGrid').click();
  assert.equal(collatorCount(), initialSorts, 'view changes must reuse sorted results');
  document.getElementById('btnViewList').click();
  const filter = document.getElementById('filterInput');
  filter.value = 'file-09999';
  filter.dispatchEvent(new window.Event('input', { bubbles: true }));
  assert.equal(document.querySelectorAll('#explorerContent tbody tr').length, 1);
  assert.ok(document.getElementById('explorerContent').textContent.includes('file-09999'));
  assert.ok(collatorCount() > initialSorts, 'filter change must invalidate cached results');
  filter.value = '';
  filter.dispatchEvent(new window.Event('input', { bubbles: true }));
  document.querySelector('[data-act="sort"][data-sort="size"]').click();
  assert.ok(document.querySelector('#explorerContent tbody tr').textContent.includes('file-00000'));
  assert.equal(document.querySelector('th[aria-sort]').getAttribute('aria-sort'), 'ascending');
  document.querySelector('[data-act="sort"][data-sort="size"]').click();
  assert.ok(document.querySelector('#explorerContent tbody tr').textContent.includes('file-09999'), 'sort direction must invalidate cache');
  assert.equal(document.querySelector('th[aria-sort]').getAttribute('aria-sort'), 'descending');
  items[0] = { ...items[0], name: 'fresh-entry.txt', size: 20000 };
  document.getElementById('btnRefresh').click();
  await eventually(() => document.querySelector('#explorerContent tbody tr')?.textContent.includes('fresh-entry.txt'), 'refresh must invalidate the cached listing');

  const beforeLanguageSorts = collatorCount();
  document.getElementById('btnLang').click();
  assert.ok(collatorCount() > beforeLanguageSorts, 'language change must invalidate cached ordering');
  assert.equal(document.documentElement.lang, 'vi', 'language switch should update document language');
  document.getElementById('btnLang').click();
  assert.equal(document.documentElement.lang, 'en');
  const beforeTheme = document.documentElement.dataset.theme || 'system';
  document.getElementById('btnTheme').click();
  assert.notEqual(document.documentElement.dataset.theme || 'system', beforeTheme, 'theme control should cycle themes');

  document.getElementById('cmdTrigger').click();
  assert.equal(document.getElementById('paletteOverlay').hidden, false, 'command palette should open');
  document.getElementById('paletteInput').value = 'needle';
  document.getElementById('paletteInput').dispatchEvent(new window.Event('input', { bubbles: true }));
  await wait(220);
  assert.ok(calls.some(call => call.path === '/api/fs/search'), 'command palette should query FTS search');
  document.getElementById('paletteInput').dispatchEvent(new window.KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
  assert.equal(document.getElementById('paletteOverlay').hidden, true);

  // Search opened from a keyboard shortcut returns focus and contains Tab.
  filter.focus();
  filter.dispatchEvent(new window.KeyboardEvent('keydown', { key: 'k', ctrlKey: true, bubbles: true, cancelable: true }));
  await eventually(() => document.activeElement === document.getElementById('paletteInput'), 'Palette should focus search');
  const first = document.getElementById('paletteInput');
  const last = document.querySelector('#paletteResults button:last-child');
  first.dispatchEvent(new window.KeyboardEvent('keydown', { key: 'Tab', shiftKey: true, bubbles: true, cancelable: true }));
  assert.equal(document.activeElement, last, 'Shift+Tab should wrap inside the palette');
  last.dispatchEvent(new window.KeyboardEvent('keydown', { key: 'Tab', bubbles: true, cancelable: true }));
  assert.equal(document.activeElement, first, 'Tab should wrap inside the palette');
  first.dispatchEvent(new window.KeyboardEvent('keydown', { key: 'Escape', bubbles: true, cancelable: true }));
  assert.equal(document.activeElement, filter, 'Closing palette should restore the initiating focus');

  document.getElementById('btnTopAdmin').click();
  await eventually(() => !document.getElementById('adminView').hidden, 'Admin view did not open');
  await eventually(() => document.querySelector('[data-act="ping-now"]'), 'Liveness action did not render');
  document.querySelector('[data-act="ping-now"]').click();
  await eventually(() => document.getElementById('pingStatus')?.textContent.includes('responded'), 'Liveness check did not show result');
  const monitor = document.getElementById('monitorOrigin');
  monitor.value = 'http://evil.test';
  document.querySelector('[data-act="monitor-connect"]').click();
  await eventually(() => document.getElementById('monitorStatus')?.textContent.includes('workers.dev'), 'Invalid monitor URL should be rejected');
  monitor.value = 'https://my-monitor.account.workers.dev';
  document.querySelector('[data-act="monitor-connect"]').click();
  await eventually(() => document.getElementById('monitorStatus')?.textContent.includes('23 of 25'), 'External monitor summary did not render');
  assert.ok(calls.some(c => c.path === '/api/admin/system/uptime' && c.query.get('origin') === monitor.value));
  for (const tab of ['overview', 'storages', 'users', 'roles', 'metas', 'shares', 'tasks', 'settings']) {
    document.querySelector(`[data-tab="${tab}"]`).click();
    await eventually(() => document.querySelector(`[data-tab="${tab}"]`).getAttribute('aria-selected') === 'true', `Admin tab ${tab} did not activate`);
    await wait(20);
    assert.ok(!document.getElementById('adminPanelBody').textContent.includes('undefined'), `${tab} rendered an undefined value`);
  }
  document.querySelector('[data-tab="storages"]').click();
  await eventually(() => document.querySelector('[data-act="storage-new"]'), 'Storage actions did not render');
  document.querySelector('[data-act="storage-new"]').click();
  await eventually(() => document.getElementById('driverFields'), 'Dynamic driver fields were not created');
  const driver = document.getElementById('storageDriver');
  driver.value = 'S3'; driver.dispatchEvent(new window.Event('change', { bubbles: true }));
  assert.ok(document.querySelector('[name="driver_field_bucket"]'), 'Driver schema should render the S3 bucket field');
  assert.ok(document.querySelector('[name="driver_field_region"]'));
  driver.value = 'Catbox'; driver.dispatchEvent(new window.Event('change', { bubbles: true }));
  assert.ok(document.querySelector('textarea[name="driver_field_files"]'));
  assert.ok(document.getElementById('driverFields').textContent.includes('Links remain public'), 'Catbox privacy warning must be visible');
  document.querySelector('[data-act="close-modal"]').click();

  document.querySelector('[data-tab="roles"]').click();
  await eventually(() => document.querySelector('[data-act="role-new"]'), 'Role actions did not render');
  document.querySelector('[data-act="role-new"]').click();
  assert.equal(document.querySelectorAll('[name^="perm_"]').length, 17, 'RBAC editor must expose all 17 permission bits');
  document.querySelector('[data-act="close-modal"]').click();

  document.querySelector('[data-tab="settings"]').click();
  await eventually(() => document.querySelector('[data-act="token-reset"]'), 'Settings tools did not render');
  document.querySelector('[data-act="token-reset"]').click();
  await eventually(() => document.getElementById('newSystemToken')?.textContent === 'one-time-system-token', 'One-time system token dialog did not render');
  assert.equal(document.getElementById('newSystemToken').textContent, 'one-time-system-token');
  window.happyDOM.abort();
}

async function testDynamicExplorerDialogs() {
  const items = Array.from({ length: 2 }, (_, i) => ({ name: i ? 'notes.md' : 'hello.txt', size: 12, is_dir: false, modified: '2026-11-16T00:00:00Z', type: 5 }));
  const { window, calls, collatorCount } = createHarness({ items });
  const { document } = window;
  await eventually(() => document.querySelectorAll('#explorerContent tbody tr').length === 2, 'Small explorer listing did not load');

  const firstRow = document.querySelector('[data-item-path="/hello.txt"]');
  firstRow.click();
  await eventually(() => document.getElementById('previewCode'), 'Text preview did not load');
  document.querySelector('[data-act="edit-preview"]').click();
  await eventually(() => document.getElementById('previewEditor'), 'Dynamic preview editor did not mount');
  document.getElementById('previewEditor').value = 'updated text';
  document.querySelector('[data-act="save-preview"]').click();
  await eventually(() => calls.some(call => call.method === 'PUT' && call.path === '/api/fs/put'), 'Save did not use streaming PUT endpoint');
  const saved = calls.find(call => call.method === 'PUT' && call.path === '/api/fs/put');
  assert.equal(saved.body, 'updated text');
  assert.equal(saved.headers['File-Path'], '%2Fhello.txt');
  await eventually(() => document.getElementById('modalOverlay').hidden && calls.filter(call => call.path === '/api/fs/list').length >= 2 && document.querySelector('[data-item-path="/notes.md"]'), 'Preview save did not complete its refresh before continuing');

  document.querySelector('[data-item-path="/notes.md"] input[type="checkbox"]').click();
  await eventually(() => !document.getElementById('batchDock').hidden, 'Batch actions did not appear after selection');
  document.querySelector('[data-act="batch-copy"]').click();
  await eventually(() => document.getElementById('pickerBody'), 'Destination picker did not mount');
  await eventually(() => document.querySelector('[data-act="picker-open"]'), `Destination picker did not load directories; body=${document.getElementById('pickerBody')?.innerHTML}; calls=${calls.map(call => call.path).join(',')}`);
  assert.equal(document.getElementById('pickerBody').textContent.includes('Documents'), true);
  window.happyDOM.abort();
}

async function testPublicShare() {
  const { window, calls } = createHarness({ path: '/s/share-123', role: 1 });
  const { document } = window;
  await eventually(() => !document.getElementById('shareView').hidden, 'Public share route was not detected');
  await eventually(() => document.querySelector('[data-form="share-auth"]'), 'Password-protected share gate did not render');
  const form = document.querySelector('[data-form="share-auth"]');
  form.querySelector('[name="password"]').value = 'share-password';
  form.dispatchEvent(new window.Event('submit', { bubbles: true, cancelable: true }));
  await eventually(() => calls.some(call => call.path === '/api/public/share/list'), 'Successful share unlock did not load its directory');
  await eventually(() => document.querySelector('[data-act="share-item-open"]'), 'Shared items did not render after unlock');
  assert.equal(document.querySelectorAll('#shareBody img').length, 0, 'untrusted shared filenames must not become HTML');
  assert.ok(document.querySelector('#shareBody').textContent.includes('<img src=x onerror=alert(1)>.txt'));
  document.querySelector('[data-item-path="/Nested"]').click();
  await eventually(() => calls.filter(call => call.path === '/api/public/share/list').length >= 2, 'Shared-folder navigation did not issue a scoped list request');
  window.happyDOM.abort();
}

async function testGuestControls() {
  const { window } = createHarness({ role: 1 });
  const { document } = window;
  await eventually(() => document.getElementById('btnAuth').textContent === 'guest', 'Guest identity was not loaded');
  assert.equal(document.getElementById('btnTopAdmin').hidden, true, 'Guests must not see the admin entry');
  document.querySelector('[data-act="open-account"]').click();
  await eventually(() => document.querySelector('[data-act="sign-out"]'), 'Guest account dialog did not open');
  assert.equal(document.querySelector('[data-act="setup-2fa"]'), null);
  assert.equal(document.querySelector('[data-act="change-password"]'), null);
  assert.equal(document.querySelector('[data-act="account-sessions"]'), null);
  window.happyDOM.abort();
}

await testExplorerAndAdmin();
await testDynamicExplorerDialogs();
await testPublicShare();
await testGuestControls();
// The source index must work as static HTML rather than shipping build markers.
const rawIndex = await readFile(new URL('../web/index.html', import.meta.url), 'utf8');
assert.match(rawIndex, /<link rel="stylesheet" href="\/styles\.css">/);
assert.match(rawIndex, /<script src="\/app\.js" defer><\/script>/);
assert.ok(!rawIndex.includes('__INLINE_'));
{
  const { window } = createHarness({ backendDown: true });
  await eventually(() => !window.document.getElementById('announcementBar').hidden, 'Missing backend warning');
  assert.match(window.document.getElementById('announcementText').textContent, /server is unavailable/i);
  assert.equal(window.document.getElementById('btnAuth').disabled, true);
  assert.equal(window.document.querySelector('[data-act="dismiss-announcement"]').hidden, true);
  window.document.getElementById('btnLang').click();
  assert.match(window.document.getElementById('announcementText').textContent, /Không kết nối được máy chủ Kova/);
  window.close();
}

console.log('Embedded minified Web UI smoke passed: virtualized explorer, EN/VI and theme controls, FTS palette, all admin tabs, 17-bit RBAC, dynamic driver/preview/picker/token DOM, public share password/XSS rendering, and guest controls.');
