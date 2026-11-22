(() => {
  'use strict';
  let token = '';
  let currentPath = '/';
  let capabilities = { create: false, delete: false, move: false, share: false };
  let listingRequest = null;
  let generation = 0;
  let shareGeneration = 0;
  let shareListGeneration = 0;
  let currentShare = '';
  const $ = id => document.getElementById(id);
  const login = $('login');
  const workspace = $('workspace');
  const message = $('message');
  const fileList = $('file-list');
  const upload = $('upload');
  const maxDownload = 128 * 1024 * 1024;

  function notice(text, isError = false) {
    message.textContent = text;
    message.hidden = !text;
    message.classList.toggle('error', isError);
  }
  function endpoint(name, path) {
    return `/api/local/${name}?${new URLSearchParams({ path })}`;
  }
  async function api(name, path, options = {}) {
    const { destination, ...requestOptions } = options;
    const target = endpoint(name, path) + (destination === undefined ? '' : `&${new URLSearchParams({ destination })}`);
    const response = await fetch(target, {
      ...requestOptions,
      headers: { Authorization: `Bearer ${token}`, ...(options.headers || {}) },
      credentials: 'omit', cache: 'no-store', redirect: 'error'
    });
    if (!response.ok) {
      const messages = { 400: 'Đường dẫn không hợp lệ.', 401: 'Token không hợp lệ.', 403: 'Không có quyền truy cập.', 404: 'Không tìm thấy tệp hoặc thư mục.', 405: 'Thao tác chưa được bật trên máy chủ.', 409: 'Tệp đã tồn tại hoặc thư mục chưa rỗng.', 413: 'Dữ liệu vượt quá giới hạn.', 429: 'Máy chủ đang xử lý quá nhiều lượt tải lên.', 503: 'Kho tệp chưa được cấu hình.' };
      throw new Error(messages[response.status] || `Yêu cầu thất bại (${response.status}).`);
    }
    return response;
  }
  function join(parent, name) { return parent === '/' ? `/${name}` : `${parent}/${name}`; }
  function formatSize(value) {
    if (!Number.isFinite(value) || value < 0) return '—';
    if (value < 1024) return `${value} B`;
    const units = ['KiB', 'MiB', 'GiB', 'TiB'];
    let amount = value / 1024, unit = 0;
    while (amount >= 1024 && unit < units.length - 1) { amount /= 1024; unit++; }
    return `${amount.toFixed(1)} ${units[unit]}`;
  }
  function renderBreadcrumbs() {
    const nav = $('breadcrumbs'); nav.replaceChildren();
    const add = (label, path) => {
      const button = document.createElement('button'); button.type = 'button'; button.textContent = label;
      button.addEventListener('click', () => load(path)); nav.append(button);
    };
    add('Kho của tôi', '/');
    let path = '';
    for (const segment of currentPath.split('/').filter(Boolean)) {
      path += `/${segment}`; const separator = document.createElement('span'); separator.textContent = '/'; nav.append(separator); add(segment, path);
    }
  }
  function render(entries) {
    fileList.replaceChildren();
    $('empty').hidden = entries.length !== 0;
    $('count').textContent = `${entries.length} mục`;
    for (const entry of entries) {
      const tr = document.createElement('tr');
      const nameCell = document.createElement('td');
      const nameButton = document.createElement('button'); nameButton.type = 'button'; nameButton.className = 'file-name';
      const icon = document.createElement('span'); icon.className = entry.is_dir ? 'file-icon folder' : 'file-icon'; icon.textContent = entry.is_dir ? '▰' : '▤';
      const name = document.createElement('span'); name.textContent = entry.name;
      nameButton.append(icon, name); nameButton.addEventListener('click', () => entry.is_dir ? load(join(currentPath, entry.name)) : download(join(currentPath, entry.name), entry.name, entry.size));
      nameCell.append(nameButton); tr.append(nameCell);
      const size = document.createElement('td'); size.textContent = entry.is_dir ? '—' : formatSize(entry.size); tr.append(size);
      const date = document.createElement('td'); const timestamp = new Date(entry.modified * 1000); date.textContent = Number.isNaN(timestamp.valueOf()) ? '—' : timestamp.toLocaleDateString('vi-VN'); tr.append(date);
      const action = document.createElement('td'); action.className = 'row-action'; const button = document.createElement('button'); button.type = 'button'; button.textContent = entry.is_dir ? 'Mở →' : 'Tải xuống ↓'; button.addEventListener('click', () => nameButton.click()); action.append(button);
      if (capabilities.share && !entry.is_dir) {
        const share = document.createElement('button'); share.type = 'button'; share.className = 'share-action'; share.textContent = 'Chia sẻ'; share.setAttribute('aria-label', `Chia sẻ ${entry.name}`);
        share.addEventListener('click', () => createShare(join(currentPath, entry.name)));
        action.append(share);
      }
      if (capabilities.move) {
        const rename = document.createElement('button'); rename.type = 'button'; rename.className = 'rename-action'; rename.textContent = 'Đổi tên'; rename.setAttribute('aria-label', `Đổi tên ${entry.name}`);
        rename.addEventListener('click', async () => {
          const dir = currentPath; const serial = generation; const newName = window.prompt('Tên mới:', entry.name);
          if (newName === null || newName === entry.name) return;
          if (!newName || newName === '.' || newName === '..' || /[\/\\\x00-\x1f\x7f]/.test(newName)) { notice('Tên không hợp lệ.', true); return; }
          try { await api('move', join(dir, entry.name), { method: 'POST', destination: join(dir, newName) }); if (currentPath === dir && generation === serial) await load(dir); }
          catch (error) { notice(error.message || 'Không thể đổi tên.', true); }
        });
        action.append(rename);
      }
      if (capabilities.delete) {
      const remove = document.createElement('button'); remove.type = 'button'; remove.className = 'delete-action'; remove.textContent = 'Xóa'; remove.setAttribute('aria-label', `Xóa ${entry.name}`); remove.addEventListener('click', async () => {
        const dir = currentPath; const serial = generation;
        if (!window.confirm(`Xóa ${entry.name}? Chỉ có thể xóa thư mục rỗng.`)) return;
        try { await api('file', join(dir, entry.name), { method: 'DELETE' }); if (currentPath === dir && generation === serial) await load(dir); }
        catch (error) { notice(error.message || 'Không thể xóa.', true); }
      });
      action.append(remove);
      }
      tr.append(action);
      fileList.append(tr);
    }
  }
  async function load(path) {
    if (listingRequest) listingRequest.abort();
    const request = new AbortController(); listingRequest = request;
    const serial = ++generation;
    notice('Đang tải...');
    try {
      const response = await api('list', path, { signal: request.signal });
      const data = await response.json();
      if (serial !== generation) return;
      if (!Array.isArray(data.entries)) throw new Error('Dữ liệu trả về không hợp lệ.');
      capabilities = { create: data.capabilities?.create === true, delete: data.capabilities?.delete === true, move: data.capabilities?.move === true, share: data.capabilities?.share === true };
      $('mkdir').hidden = !capabilities.create;
      $('upload-control').hidden = !capabilities.create;
      $('share-manage').hidden = !capabilities.share;
      currentPath = path; $('current-path').textContent = path;
      renderBreadcrumbs(); render(data.entries); notice('');
      login.hidden = true; workspace.hidden = false; $('logout').hidden = false;
    } catch (error) {
      if (error.name !== 'AbortError' && serial === generation) notice(error.message || 'Không thể kết nối.', true);
    }
  }
  function clearShare() {
    shareGeneration++; currentShare = ''; $('share-result').hidden = true; $('share-url').value = ''; $('share-expiry').textContent = '';
  }
  function showShare(data) {
    if (typeof data.url !== 'string' || !/^\/s\/[A-Za-z0-9_-]{43}$/.test(data.url) || !Number.isSafeInteger(data.expires) || !Number.isFinite(new Date(data.expires * 1000).valueOf())) throw new Error('Máy chủ trả về liên kết không hợp lệ.');
    currentShare = data.url.slice(3);
    $('share-url').value = window.location.origin + data.url;
    $('share-expiry').textContent = `Hết hạn: ${new Date(data.expires * 1000).toLocaleString('vi-VN')}.`;
    $('share-result').hidden = false; notice(''); $('share-url').focus(); $('share-url').select();
  }
  async function loadShares() {
    const serial = ++shareListGeneration;
    $('share-list-panel').hidden = false; $('share-list').textContent = 'Đang tải...';
    try {
      const response = await api('shares', '/'); const data = await response.json();
      if (serial !== shareListGeneration) return;
      if (!Array.isArray(data.shares) || data.shares.length > 1000) throw new Error('Danh sách liên kết không hợp lệ.');
      const list = $('share-list'); list.replaceChildren();
      if (!data.shares.length) { list.textContent = 'Chưa có liên kết đang hoạt động.'; return; }
      for (const item of data.shares) {
        if (typeof item.path !== 'string' || typeof item.url !== 'string' || !/^\/s\/[A-Za-z0-9_-]{43}$/.test(item.url) || !Number.isSafeInteger(item.expires) || !Number.isFinite(new Date(item.expires * 1000).valueOf())) throw new Error('Danh sách liên kết không hợp lệ.');
        const row = document.createElement('div'); row.className = 'share-list-row';
        const filename = document.createElement('span'); filename.className = 'share-list-path'; filename.textContent = item.path;
        const expires = document.createElement('time'); expires.dateTime = new Date(item.expires * 1000).toISOString(); expires.textContent = new Date(item.expires * 1000).toLocaleString('vi-VN');
        const view = document.createElement('button'); view.type = 'button'; view.className = 'button secondary share-list-view'; view.textContent = 'Xem liên kết'; view.setAttribute('aria-label', `Xem liên kết ${item.path}`);
        view.addEventListener('click', () => { clearShare(); showShare(item); });
        row.append(filename, expires, view); list.append(row);
      }
    } catch (error) { if (serial === shareListGeneration) { $('share-list').textContent = ''; notice(error.message || 'Không thể tải danh sách.', true); } }
  }
  async function createShare(filePath) {
    clearShare(); const serial = shareGeneration;
    notice('Đang tạo liên kết...');
    try {
      const response = await api('shares', '/', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ path: filePath, ttl_seconds: 3600 }) });
      const data = await response.json();
      if (serial !== shareGeneration) return;
      showShare(data);
      if (!$('share-list-panel').hidden) loadShares();
    } catch (error) { if (serial === shareGeneration) notice(error.message || 'Không thể tạo liên kết.', true); }
  }
  async function download(path, name, size) {
    if (size > maxDownload) { notice('Tải bằng trình duyệt giới hạn 128 MiB. Dùng API để tải tệp lớn hơn.', true); return; }
    notice('Đang tải tệp...');
    try {
      const response = await api('file', path);
      const length = Number(response.headers.get('Content-Length'));
      if (length > maxDownload) { await response.body?.cancel(); throw new Error('Tệp vượt giới hạn tải của trình duyệt (128 MiB).'); }
      const reader = response.body?.getReader();
      if (!reader) throw new Error('Trình duyệt không hỗ trợ tải tệp này.');
      const chunks = []; let total = 0;
      try {
        while (true) {
          const { value, done } = await reader.read(); if (done) break;
          total += value.byteLength;
          if (total > maxDownload) { await reader.cancel(); throw new Error('Tệp vượt giới hạn tải của trình duyệt (128 MiB).'); }
          chunks.push(value);
        }
      } finally { reader.releaseLock(); }
      const url = URL.createObjectURL(new Blob(chunks, { type: 'application/octet-stream' }));
      const a = document.createElement('a'); a.href = url; a.download = name; document.body.append(a); a.click(); a.remove();
      setTimeout(() => URL.revokeObjectURL(url), 30000); notice('Đã tải tệp.');
    } catch (error) { notice(error.message || 'Không thể tải tệp.', true); }
  }
  $('login-form').addEventListener('submit', event => {
    event.preventDefault(); token = $('token').value; $('token').value = '';
    if (token.length < 32) { token = ''; notice('Token phải có ít nhất 32 ký tự.', true); return; }
    load('/');
  });
  $('logout').addEventListener('click', () => {
    token = ''; generation++; clearShare(); shareListGeneration++; $('share-list-panel').hidden = true; $('share-list').replaceChildren(); if (listingRequest) listingRequest.abort(); listingRequest = null;
    currentPath = '/'; capabilities = { create: false, delete: false, move: false, share: false }; $('mkdir').hidden = true; $('upload-control').hidden = true; $('share-manage').hidden = true; fileList.replaceChildren(); workspace.hidden = true; login.hidden = false; $('logout').hidden = true; notice('Đã đăng xuất.'); $('token').focus();
  });
  $('share-manage').addEventListener('click', loadShares);
  $('share-list-close').addEventListener('click', () => { shareListGeneration++; $('share-list-panel').hidden = true; $('share-list').replaceChildren(); });
  $('share-close').addEventListener('click', clearShare);
  $('share-copy').addEventListener('click', async () => {
    const serial = shareGeneration;
    try { if (!navigator.clipboard?.writeText) throw new Error('Không thể sao chép tự động. Hãy chọn và sao chép liên kết.'); await navigator.clipboard.writeText($('share-url').value); if (serial === shareGeneration) notice('Đã sao chép liên kết.'); }
    catch (error) { if (serial === shareGeneration) notice(error.message || 'Hãy chọn và sao chép liên kết.', true); }
  });
  $('share-revoke').addEventListener('click', async () => {
    const id = currentShare; const serial = shareGeneration; if (!id) return;
    $('share-revoke').disabled = true;
    try { await api(`shares/${id}`, '/', { method: 'DELETE' }); if (serial === shareGeneration) { clearShare(); notice('Đã thu hồi liên kết.'); if (!$('share-list-panel').hidden) loadShares(); } }
    catch (error) { if (serial === shareGeneration) notice(error.message || 'Không thể thu hồi.', true); }
    finally { $('share-revoke').disabled = false; }
  });
  $('refresh').addEventListener('click', () => load(currentPath));
  $('mkdir').addEventListener('click', async () => {
    const name = window.prompt('Tên thư mục mới:');
    if (name === null) return;
    if (!name || name === '.' || name === '..' || /[\/\\\x00-\x1f\x7f]/.test(name)) { notice('Tên thư mục không hợp lệ.', true); return; }
    const dir = currentPath; const serial = generation;
    try { await api('mkdir', join(dir, name), { method: 'POST' }); if (currentPath === dir && generation === serial) await load(dir); }
    catch (error) { notice(error.message || 'Không thể tạo thư mục.', true); }
  });
  upload.addEventListener('change', async () => {
    const file = upload.files[0]; if (!file) return;
    upload.value = '';
    if (file.size > 64 * 1024 * 1024) { notice('Tệp vượt giới hạn 64 MiB.', true); return; }
    const dir = currentPath; const serial = generation; notice('Đang tải lên...');
    try { await api('file', join(dir, file.name), { method: 'PUT', body: file }); if (generation === serial) { notice('Đã tải lên.'); if (currentPath === dir) await load(dir); } }
    catch (error) { notice(error.message || 'Không thể tải lên.', true); }
  });
})();
