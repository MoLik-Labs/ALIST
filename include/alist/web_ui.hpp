#pragma once

#include <string_view>

namespace alist {

// Self-contained, zero-CDN responsive Single-Page Application embedded directly in the binary.
// Supports File Explorer, Breadcrumbs, Instant FTS5 Search, Streaming Upload, Video/Audio/Image/Text/ZIP Preview,
// Public Share Links (/s/:share_id), 2FA, and Full Admin Console (11 Drivers, Users, Roles, Metas, Shares, Tasks, Settings).
inline constexpr std::string_view kEmbeddedWebUiHtml = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>AList @nggkhaiz · Storage</title>
<link rel="icon" type="image/png" href="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAB00lEQVR4AeyVuy8EURTG1zMKBQUaJJ4FohcKBVqPQqGgEDoqQkFIaLwaOioaQYFWNBL8AUgIiZKIaISIxOt3kh13xtyxszP7Kmbz/XLuuffMmW/vzN5NDyX5ExhIyR345rWIJ7RXSskdUPYSMAp2INgB3Q6k8e55pYZrD0GnSyZbwSKdAUuBy6SAulU4hxYw64FkEOrAZs6vgRyaTsAtDEAGGHpjMAsVsAZfYJNXA/KIeul2AzOQC4bkFN0gqYRJeAVHeTHQSLczWIdiMOuEpB764A4iKhoDspV7dDwGeZ6EX10z6oAmuADXcmMgn27LcAXtYNYTyTDUwj5Erf8MZNNtBOQFGyJmgqF3BotQDivwAZ7kZKCbbvKNF4h5YNY2SRWMwjP4ks7AKR23oAz+6oAJOVD6idMe4TIlnYEGtWwbtTEz5RMuV9IZUKsJGAUGnHZAflabPIFOkJ+fHL1CFnkXyNon0bd0BpboWgo9ICef+UZibJd5WSshSu0L0a1stToDcvjcu+goNVJbTa0YeSQ6SdakRmotNToDlgIXiWGkiFo5wOaJR2HmiDJXSBSzUstQKRYGjG7yN7xDMgbNYcaJMkfQK5YG9HeIMBt3AxHuH/oBAAD//zKEaE0AAAAGSURBVAMAMcNrQerOH9kAAAAASUVORK5CYII=">
<style>
:root{color-scheme:dark;--bg:#090a0b;--panel:#111315;--panel2:#191c1f;--border:#2a2e32;--text:#e8eaec;--muted:#92999f;--accent:#d2d7db;--accent2:#fff;--ok:#6dbf8b;--danger:#e27676;--warn:#d5ae63}
[data-theme="light"]{color-scheme:light;--bg:#f4f5f5;--panel:#fff;--panel2:#f0f1f1;--border:#d9dcde;--text:#17191a;--muted:#656b70;--accent:#30363a;--accent2:#111;--ok:#267a47;--danger:#bd3434;--warn:#936b19}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Ubuntu,sans-serif;background:var(--bg);color:var(--text);min-height:100vh;line-height:1.5}
a{color:inherit;text-decoration:none}
button,input,select,textarea{font:inherit;color:var(--text)}
.topbar{position:sticky;top:0;z-index:30;display:flex;align-items:center;justify-content:space-between;gap:12px;padding:12px 24px;background:var(--panel);border-bottom:1px solid var(--border);backdrop-filter:blur(12px)}
.brand{display:flex;align-items:center;gap:10px;font-weight:700;font-size:1.1rem;cursor:pointer}
.brand-logo{width:30px;height:30px;object-fit:contain;filter:invert(1)}
[data-theme="light"] .brand-logo{filter:none}
.badge{font-size:.7rem;padding:2px 8px;border-radius:99px;background:rgba(99,102,241,.18);color:#818cf8;border:1px solid rgba(99,102,241,.35);font-weight:600}
.actions{display:flex;align-items:center;gap:8px;flex-wrap:wrap}
.btn{padding:7px 13px;border-radius:8px;border:1px solid var(--border);background:var(--panel2);cursor:pointer;font-size:.86rem;font-weight:500;transition:.15s;display:inline-flex;align-items:center;gap:6px}
.btn:hover{border-color:var(--accent)}
.btn-primary{background:var(--accent);border-color:var(--accent);color:#fff}
.btn-primary:hover{background:var(--accent2)}
.btn-danger{background:rgba(239,68,68,.15);border-color:rgba(239,68,68,.4);color:var(--danger)}
.container{max-width:1240px;margin:20px auto;padding:0 20px}
.card{background:var(--panel);border:1px solid var(--border);border-radius:14px;overflow:hidden;margin-bottom:18px;box-shadow:0 8px 28px rgba(0,0,0,.18)}
.crumbs{display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:10px;padding:12px 18px;border-bottom:1px solid var(--border);background:var(--panel2)}
.crumb-list{display:flex;align-items:center;flex-wrap:wrap;gap:4px;font-size:.92rem}
.crumb{padding:4px 8px;border-radius:6px;cursor:pointer;color:var(--muted)}
.crumb:hover,.crumb.active{color:var(--text);background:rgba(99,102,241,.15)}
.toolbar{display:flex;gap:8px;flex-wrap:wrap;padding:12px 18px;border-bottom:1px solid var(--border);align-items:center}
.input{padding:8px 12px;border-radius:8px;border:1px solid var(--border);background:var(--bg);color:var(--text);font-size:.88rem;width:100%}
.input:focus{outline:none;border-color:var(--accent)}
table{width:100%;border-collapse:collapse}
th,td{padding:11px 18px;text-align:left;border-bottom:1px solid var(--border);font-size:.9rem}
th{color:var(--muted);font-weight:600;font-size:.8rem;text-transform:uppercase;letter-spacing:.04em;cursor:pointer;user-select:none}
tr:last-child td{border-bottom:none}
tbody tr{transition:background .12s;cursor:pointer}
tbody tr:hover{background:var(--panel2)}
.fname{display:flex;align-items:center;gap:10px;font-weight:500;word-break:break-all}
.ficon{width:26px;text-align:center;font-size:1.1rem;flex-shrink:0}
.row-ops{display:flex;gap:6px;justify-content:flex-end}
.row-ops button{padding:4px 8px;font-size:.76rem}
.empty{padding:48px 20px;text-align:center;color:var(--muted)}
.modal-bg{position:fixed;inset:0;background:rgba(0,0,0,.68);display:none;align-items:center;justify-content:center;z-index:60;padding:16px}
.modal-bg.open{display:flex}
.modal{background:var(--panel);border:1px solid var(--border);border-radius:14px;width:100%;max-width:680px;max-height:90vh;overflow:auto;padding:22px}
.modal h3{margin-bottom:14px;font-size:1.1rem}
.field{margin-bottom:12px}
.field label{display:block;font-size:.82rem;color:var(--muted);margin-bottom:4px}
.tabs{display:flex;gap:6px;padding:12px 18px;border-bottom:1px solid var(--border);flex-wrap:wrap;background:var(--panel2)}
.tab{padding:6px 12px;border-radius:8px;cursor:pointer;font-size:.86rem;color:var(--muted)}
.tab.active{background:var(--accent);color:#fff;font-weight:600}
.toast{position:fixed;bottom:20px;right:20px;padding:10px 16px;border-radius:10px;background:var(--panel2);border:1px solid var(--accent);box-shadow:0 8px 24px rgba(0,0,0,.35);z-index:99;display:none;font-size:.88rem}
.preview-box{padding:18px}
.preview-box video,.preview-box audio,.preview-box img{max-width:100%;border-radius:10px;display:block;margin:0 auto}
pre.code-view{background:var(--bg);padding:16px;border-radius:10px;overflow:auto;max-height:520px;font-size:.84rem;border:1px solid var(--border);white-space:pre-wrap;word-break:break-word}
.prog{height:6px;background:var(--border);border-radius:99px;overflow:hidden;margin-top:8px}
.prog > div{height:100%;background:var(--ok);width:0%;transition:width .2s}
.readme-card{padding:18px 22px;font-size:.92rem;white-space:pre-wrap}
.grid-2{display:grid;grid-template-columns:1fr 1fr;gap:12px}
@media(max-width:720px){.grid-2{grid-template-columns:1fr}th:nth-child(3),td:nth-child(3){display:none}}
</style>
</head>
<body>
<header class="topbar">
  <div class="brand" onclick="navTo('/')">
    <img class="brand-logo" alt="@nggkhaiz logo" width="30" height="30" src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAB00lEQVR4AeyVuy8EURTG1zMKBQUaJJ4FohcKBVqPQqGgEDoqQkFIaLwaOioaQYFWNBL8AUgIiZKIaISIxOt3kh13xtyxszP7Kmbz/XLuuffMmW/vzN5NDyX5ExhIyR345rWIJ7RXSskdUPYSMAp2INgB3Q6k8e55pYZrD0GnSyZbwSKdAUuBy6SAulU4hxYw64FkEOrAZs6vgRyaTsAtDEAGGHpjMAsVsAZfYJNXA/KIeul2AzOQC4bkFN0gqYRJeAVHeTHQSLczWIdiMOuEpB764A4iKhoDspV7dDwGeZ6EX10z6oAmuADXcmMgn27LcAXtYNYTyTDUwj5Erf8MZNNtBOQFGyJmgqF3BotQDivwAZ7kZKCbbvKNF4h5YNY2SRWMwjP4ks7AKR23oAz+6oAJOVD6idMe4TIlnYEGtWwbtTEz5RMuV9IZUKsJGAUGnHZAflabPIFOkJ+fHL1CFnkXyNon0bd0BpboWgo9ICef+UZibJd5WSshSu0L0a1stToDcvjcu+goNVJbTa0YeSQ6SdakRmotNToDlgIXiWGkiFo5wOaJR2HmiDJXSBSzUstQKRYGjG7yN7xDMgbNYcaJMkfQK5YG9HeIMBt3AxHuH/oBAAD//zKEaE0AAAAGSURBVAMAMcNrQerOH9kAAAAASUVORK5CYII=">
    <span id="siteTitle">AList @nggkhaiz</span>
    <span class="badge">v4.1.0</span>
  </div>
  <div class="actions">
    <button class="btn" onclick="openSearch()">🔍 Search</button>
    <button class="btn" onclick="toggleTheme()" title="Switch Theme">🌓</button>
    <span id="userBadge" style="font-size:.84rem;color:var(--muted)"></span>
    <button class="btn" id="btnAdmin" style="display:none" onclick="toggleAdminView()">⚙️ Admin</button>
    <button class="btn btn-primary" id="btnAuth" onclick="openLogin()">Sign In</button>
  </div>
</header>

<main class="container">
  <!-- PUBLIC SHARE VIEW -->
  <section id="shareView" style="display:none">
    <div class="card">
      <div class="crumbs">
        <div class="crumb-list" id="shareCrumbs"></div>
        <span class="badge" id="shareMetaBadge">Public Share</span>
      </div>
      <div id="shareBody"></div>
    </div>
  </section>

  <!-- EXPLORER VIEW -->
  <section id="explorerView">
    <div class="card">
      <div class="crumbs">
        <div class="crumb-list" id="breadcrumbs"></div>
        <div style="display:flex;gap:6px">
          <button class="btn" onclick="loadPath(state.path, true)">↻ Refresh</button>
        </div>
      </div>
      <div class="toolbar" id="writeToolbar" style="display:none">
        <label class="btn btn-primary" style="cursor:pointer">
          ⬆ Upload Files
          <input type="file" id="fileInput" multiple style="display:none" onchange="uploadFiles(this.files)">
        </label>
        <button class="btn" onclick="promptMkdir()">📁 New Folder</button>
        <button class="btn" onclick="promptOfflineDl()">🌐 Offline URL</button>
        <div id="uploadStatus" style="flex:1;min-width:180px;display:none">
          <div style="font-size:.78rem;color:var(--muted)" id="uploadLabel">Uploading...</div>
          <div class="prog"><div id="uploadBar"></div></div>
        </div>
      </div>
      <div id="explorerContent"></div>
    </div>
    <div class="card" id="readmeCard" style="display:none">
      <div class="crumbs"><strong>📄 README</strong></div>
      <div class="readme-card" id="readmeBody"></div>
    </div>
  </section>

  <!-- ADMIN CONSOLE VIEW -->
  <section id="adminView" style="display:none">
    <div class="card">
      <div class="tabs">
        <div class="tab active" onclick="switchAdminTab('storages',this)">🗄 Storages</div>
        <div class="tab" onclick="switchAdminTab('users',this)">👤 Users</div>
        <div class="tab" onclick="switchAdminTab('roles',this)">🛡 Roles</div>
        <div class="tab" onclick="switchAdminTab('metas',this)">🔒 Path Metas</div>
        <div class="tab" onclick="switchAdminTab('shares',this)">🔗 Shares</div>
        <div class="tab" onclick="switchAdminTab('tasks',this)">⚡ Tasks</div>
        <div class="tab" onclick="switchAdminTab('settings',this)">⚙ Settings & Search</div>
        <div class="tab" onclick="toggleAdminView()" style="margin-left:auto">← Back to Files</div>
      </div>
      <div id="adminPanelBody" style="padding:18px"></div>
    </div>
  </section>
</main>

<div class="modal-bg" id="modalBg" onclick="if(event.target===this)closeModal()">
  <div class="modal" id="modalContent"></div>
</div>
<div class="toast" id="toast"></div>

<script>
const state = {
  path: '/',
  token: localStorage.getItem('alist_token') || '',
  clientId: localStorage.getItem('alist_cid') || (() => { const id = Math.random().toString(36).slice(2); localStorage.setItem('alist_cid', id); return id; })(),
  me: null,
  dirPasswords: {},
  entries: [],
  sortCol: 'name',
  sortAsc: true,
  adminOpen: false,
  drivers: [],
  shareId: location.pathname.startsWith('/s/') ? location.pathname.split('/')[2] : '',
  shareSub: '/',
  shareToken: sessionStorage.getItem('share_tok') || ''
};

function esc(s) {
  return String(s ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
}
function fmtSize(b, isDir) {
  if (isDir) return '-';
  const u = ['B','KB','MB','GB','TB'];
  let i = 0, v = Number(b || 0);
  while (v >= 1024 && i < u.length - 1) { v /= 1024; i++; }
  return (i === 0 ? v : v.toFixed(1)) + ' ' + u[i];
}
function iconFor(e) {
  if (e.is_dir) return '📁';
  const m = {2:'📕',3:'🎬',4:'🎵',5:'📝',6:'🖼',7:'🗜'};
  return m[e.type] || '📄';
}
function toast(msg) {
  const el = document.getElementById('toast');
  el.textContent = msg;
  el.style.display = 'block';
  clearTimeout(toast._t);
  toast._t = setTimeout(() => el.style.display = 'none', 3200);
}
let modalResultResolver = null;
function openModal(html) {
  if (modalResultResolver) closeModal(null);
  document.getElementById('modalContent').innerHTML = html;
  document.getElementById('modalBg').classList.add('open');
}
function closeModal(result = null) {
  document.getElementById('modalBg').classList.remove('open');
  const resolve = modalResultResolver;
  modalResultResolver = null;
  if (resolve) resolve(result);
}
function askText(title, label, initial = '', placeholder = '') {
  return new Promise(resolve => {
    openModal(`<h3>${esc(title)}</h3><div class="field"><label for="modalTextValue">${esc(label)}</label><input class="input" id="modalTextValue" value="${esc(initial)}" placeholder="${esc(placeholder)}"></div><div style="display:flex;gap:8px;justify-content:flex-end"><button class="btn" onclick="closeModal(null)">Cancel</button><button class="btn btn-primary" onclick="closeModal(document.getElementById('modalTextValue').value)">Continue</button></div>`);
    modalResultResolver = resolve;
    const input = document.getElementById('modalTextValue');
    input.onkeydown = event => { if (event.key === 'Enter') closeModal(input.value); };
    input.focus();
    input.select();
  });
}
function askConfirm(title, message, action = 'Confirm') {
  return new Promise(resolve => {
    openModal(`<h3>${esc(title)}</h3><p style="margin:0 0 20px;color:var(--muted)">${esc(message)}</p><div style="display:flex;gap:8px;justify-content:flex-end"><button class="btn" onclick="closeModal(false)">Cancel</button><button class="btn btn-danger" onclick="closeModal(true)">${esc(action)}</button></div>`);
    modalResultResolver = resolve;
  });
}
function toggleTheme() {
  const cur = document.documentElement.getAttribute('data-theme');
  const next = cur === 'light' ? 'dark' : 'light';
  document.documentElement.setAttribute('data-theme', next);
  localStorage.setItem('alist_theme', next);
}
if (localStorage.getItem('alist_theme') === 'light') document.documentElement.setAttribute('data-theme', 'light');

async function api(url, opts = {}) {
  const headers = Object.assign({'Client-Id': state.clientId}, opts.headers || {});
  if (state.token) headers['Authorization'] = state.token;
  if (opts.json !== undefined) {
    headers['Content-Type'] = 'application/json';
    opts.body = JSON.stringify(opts.json);
  }
  const r = await fetch(url, Object.assign({}, opts, {headers}));
  return r.json();
}

async function initApp() {
  const pub = await api('/api/public/settings');
  if (pub.code === 200 && pub.data && pub.data.site_title) {
    document.getElementById('siteTitle').textContent = pub.data.site_title;
    document.title = pub.data.site_title + ' · AList @nggkhaiz';
  }
  if (state.shareId) {
    document.getElementById('explorerView').style.display = 'none';
    document.getElementById('shareView').style.display = 'block';
    await loadPublicShare('/');
    return;
  }
  await refreshMe();
  const initPath = decodeURIComponent(location.hash.replace(/^#/, '') || '/');
  await loadPath(initPath, false);
}

async function refreshMe() {
  const r = await api('/api/me');
  if (r.code === 200 && r.data) {
    state.me = r.data;
    const isGuest = r.data.role === 1;
    document.getElementById('userBadge').textContent = isGuest ? 'Guest' : ('👤 ' + r.data.username);
    document.getElementById('btnAuth').textContent = isGuest ? 'Sign In' : 'Sign Out';
    document.getElementById('btnAuth').onclick = isGuest ? openLogin : doLogout;
    document.getElementById('btnAdmin').style.display = (r.data.role === 2) ? 'inline-flex' : 'none';
  } else {
    state.me = null;
    document.getElementById('userBadge').textContent = '';
    document.getElementById('btnAuth').textContent = 'Sign In';
    document.getElementById('btnAuth').onclick = openLogin;
    document.getElementById('btnAdmin').style.display = 'none';
  }
}

function openLogin() {
  openModal(`<h3>Sign In to AList @nggkhaiz</h3>
    <div class="field"><label>Username</label><input class="input" id="lgUser" value="admin"></div>
    <div class="field"><label>Password</label><input class="input" type="password" id="lgPass" onkeydown="if(event.key==='Enter')submitLogin()"></div>
    <div class="field"><label>2FA OTP Code (if enabled)</label><input class="input" id="lgOtp" placeholder="Optional 6-digit TOTP"></div>
    <div style="display:flex;gap:8px;justify-content:flex-end;margin-top:14px">
      <button class="btn" onclick="closeModal()">Cancel</button>
      <button class="btn btn-primary" onclick="submitLogin()">Sign In</button>
    </div>`);
  setTimeout(() => document.getElementById('lgPass')?.focus(), 50);
}

async function submitLogin() {
  const username = document.getElementById('lgUser').value.trim();
  const password = document.getElementById('lgPass').value;
  const otp_code = document.getElementById('lgOtp').value.trim();
  const r = await api('/api/auth/login', {method:'POST', json:{username, password, otp_code}});
  if (r.code !== 200) { toast('❌ ' + (r.message || 'Login failed')); return; }
  state.token = r.data.token;
  localStorage.setItem('alist_token', state.token);
  closeModal();
  toast('✅ Signed in as ' + username);
  await refreshMe();
  await loadPath(state.path, true);
}

async function doLogout() {
  await api('/api/auth/logout');
  state.token = '';
  localStorage.removeItem('alist_token');
  state.adminOpen = false;
  document.getElementById('adminView').style.display = 'none';
  document.getElementById('explorerView').style.display = 'block';
  toast('Signed out');
  await refreshMe();
  await loadPath('/', false);
}

function joinPath(a, b) {
  const s = (a === '/' ? '' : a) + '/' + b;
  return s.replace(/\/+/g, '/');
}

function navTo(p) {
  if (state.adminOpen) toggleAdminView();
  location.hash = encodeURIComponent(p).replace(/%2F/g, '/');
  loadPath(p, false);
}

function renderBreadcrumbs(p) {
  const el = document.getElementById('breadcrumbs');
  const segs = p.split('/').filter(Boolean);
  let html = `<span class="crumb ${segs.length===0?'active':''}" onclick="navTo('/')">🏠 Home</span>`;
  let cur = '';
  segs.forEach((s, idx) => {
    cur += '/' + s;
    const target = cur;
    html += `<span style="color:var(--muted)">/</span><span class="crumb ${idx===segs.length-1?'active':''}" onclick="navTo('${esc(target)}')">${esc(s)}</span>`;
  });
  el.innerHTML = html;
}

async function loadPath(p, refresh) {
  state.path = p || '/';
  renderBreadcrumbs(state.path);
  const pw = state.dirPasswords[state.path] || '';
  const r = await api('/api/fs/list', {method:'POST', json:{path: state.path, password: pw, refresh: !!refresh, page:1, per_page:0}});
  const box = document.getElementById('explorerContent');
  if (r.code === 401) {
    box.innerHTML = `<div class="empty"><p style="margin-bottom:12px">🔒 Sign in is required to browse files.</p><button class="btn btn-primary" onclick="openLogin()">Sign In</button></div>`;
    document.getElementById('writeToolbar').style.display = 'none';
    return;
  }
  if (r.code === 403 && (r.message || '').toLowerCase().includes('password')) {
    box.innerHTML = `<div class="empty"><p style="margin-bottom:12px">🔑 This folder is protected by a password.</p>
      <div style="max-width:300px;margin:0 auto;display:flex;gap:8px">
        <input class="input" type="password" id="dirPwIn" placeholder="Folder password">
        <button class="btn btn-primary" onclick="state.dirPasswords[state.path]=document.getElementById('dirPwIn').value;loadPath(state.path,false)">Unlock</button>
      </div></div>`;
    return;
  }
  if (r.code !== 200) {
    box.innerHTML = `<div class="empty">⚠️ ${esc(r.message || 'Unable to load directory')}</div>`;
    return;
  }
  const d = r.data || {};
  state.entries = d.content || [];
  document.getElementById('writeToolbar').style.display = d.write ? 'flex' : 'none';
  if (d.readme) {
    document.getElementById('readmeCard').style.display = 'block';
    document.getElementById('readmeBody').textContent = d.readme;
  } else {
    document.getElementById('readmeCard').style.display = 'none';
  }
  renderEntries();
}

function sortBy(col) {
  if (state.sortCol === col) state.sortAsc = !state.sortAsc;
  else { state.sortCol = col; state.sortAsc = true; }
  renderEntries();
}

function renderEntries() {
  const box = document.getElementById('explorerContent');
  if (!state.entries.length) {
    box.innerHTML = `<div class="empty">📭 Empty directory. ${state.me?.role===2 && state.path==='/' ? 'Open <b>⚙️ Admin → Storages</b> to mount Local, S3, WebDAV, Cloud, or Crypt storage.' : ''}</div>`;
    return;
  }
  const list = [...state.entries].sort((a, b) => {
    if (a.is_dir !== b.is_dir) return a.is_dir ? -1 : 1;
    let c = 0;
    if (state.sortCol === 'size') c = (a.size || 0) - (b.size || 0);
    else if (state.sortCol === 'modified') c = String(a.modified).localeCompare(String(b.modified));
    else c = a.name.localeCompare(b.name);
    return state.sortAsc ? c : -c;
  });
  const canManage = state.me && state.me.role !== 1;
  let rows = list.map(e => {
    const full = joinPath(state.path, e.name);
    const dlUrl = '/d' + encodeURI(full) + (e.sign ? ('?sign=' + encodeURIComponent(e.sign)) : '');
    return `<tr onclick="openEntry('${esc(e.name)}', ${!!e.is_dir})">
      <td><div class="fname"><span class="ficon">${iconFor(e)}</span><span>${esc(e.name)}</span></div></td>
      <td style="width:120px;color:var(--muted)">${fmtSize(e.size, e.is_dir)}</td>
      <td style="width:190px;color:var(--muted)">${esc((e.modified||'').replace('T',' ').replace('Z',''))}</td>
      <td style="width:210px" onclick="event.stopPropagation()"><div class="row-ops">
        ${!e.is_dir ? `<a class="btn" href="${dlUrl}" download>⬇</a>` : ''}
        ${canManage ? `<button class="btn" title="Share" onclick="openCreateShare('${esc(full)}',${!!e.is_dir})">🔗</button>
        <button class="btn" title="Rename" onclick="promptRename('${esc(full)}','${esc(e.name)}')">✏️</button>
        <button class="btn btn-danger" title="Delete" onclick="confirmDelete('${esc(full)}','${esc(e.name)}')">🗑</button>` : ''}
      </div></td>
    </tr>`;
  }).join('');
  box.innerHTML = `<table><thead><tr>
    <th onclick="sortBy('name')">Name</th>
    <th onclick="sortBy('size')">Size</th>
    <th onclick="sortBy('modified')">Modified</th>
    <th style="text-align:right">Actions</th>
  </tr></thead><tbody>${rows}</tbody></table>`;
}

async function openEntry(name, isDir) {
  const full = joinPath(state.path, name);
  if (isDir) { navTo(full); return; }
  const pw = state.dirPasswords[state.path] || '';
  const r = await api('/api/fs/get', {method:'POST', json:{path: full, password: pw}});
  if (r.code !== 200) { toast('❌ ' + r.message); return; }
  const d = r.data;
  const rawUrl = d.raw_url || ('/d' + encodeURI(full) + (d.sign ? '?sign=' + encodeURIComponent(d.sign) : ''));
  const proxyUrl = '/p' + encodeURI(full) + (d.sign ? '?sign=' + encodeURIComponent(d.sign) : '');
  let body = '';
  if (d.type === 3) {
    body = `<video controls autoplay src="${proxyUrl}"></video>`;
  } else if (d.type === 4) {
    body = `<audio controls autoplay src="${proxyUrl}" style="width:100%"></audio>`;
  } else if (d.type === 6) {
    body = `<img src="${proxyUrl}" alt="${esc(d.name)}">`;
  } else if (d.type === 5 && d.size <= 524288) {
    const txt = await (await fetch(proxyUrl)).text();
    body = `<pre class="code-view">${esc(txt)}</pre>`;
  } else if (name.toLowerCase().endsWith('.zip')) {
    const ar = await api('/api/fs/archive/list', {method:'POST', json:{path: full, password: pw}});
    if (ar.code === 200 && ar.data?.content) {
      body = `<div style="margin-bottom:8px;color:var(--muted)">🗜 ZIP Archive Contents (${ar.data.content.length} items):</div>
        <div style="max-height:360px;overflow:auto;border:1px solid var(--border);border-radius:8px">` +
        ar.data.content.map(z => `<div style="display:flex;justify-content:space-between;padding:8px 12px;border-bottom:1px solid var(--border);font-size:.85rem">
          <span>${z.is_dir?'📁':'📄'} ${esc(z.path)}</span>
          <span>${fmtSize(z.size, z.is_dir)} ${!z.is_dir ? `<a class="btn" style="padding:2px 8px;margin-left:8px" href="/ad${encodeURI(full)}?inner=${encodeURIComponent(z.path)}&sign=${encodeURIComponent(z.sign||'')}">Extract</a>` : ''}</span>
        </div>`).join('') + `</div>`;
    }
  } else {
    body = `<p style="color:var(--muted);padding:16px 0">File size: <b>${fmtSize(d.size,false)}</b> · Provider: <b>${esc(d.provider)}</b></p>`;
  }
  openModal(`<h3>${iconFor(d)} ${esc(d.name)}</h3>
    <div class="preview-box">${body}</div>
    <div style="display:flex;gap:8px;justify-content:flex-end;margin-top:14px">
      <button class="btn" onclick="navigator.clipboard?.writeText(location.origin+'${rawUrl}');toast('Copied direct link!')">📋 Copy Link</button>
      <a class="btn btn-primary" href="${rawUrl}" download>⬇ Download</a>
      <button class="btn" onclick="closeModal()">Close</button>
    </div>`);
}

async function uploadFiles(files) {
  if (!files || !files.length) return;
  const stBox = document.getElementById('uploadStatus');
  const bar = document.getElementById('uploadBar');
  const lbl = document.getElementById('uploadLabel');
  stBox.style.display = 'block';
  for (let i = 0; i < files.length; i++) {
    const f = files[i];
    const target = joinPath(state.path, f.name);
    lbl.textContent = `Uploading (${i+1}/${files.length}): ${f.name}`;
    bar.style.width = '0%';
    await new Promise((resolve) => {
      const xhr = new XMLHttpRequest();
      xhr.open('PUT', '/api/fs/put');
      if (state.token) xhr.setRequestHeader('Authorization', state.token);
      xhr.setRequestHeader('Client-Id', state.clientId);
      xhr.setRequestHeader('File-Path', encodeURIComponent(target));
      if (state.dirPasswords[state.path]) xhr.setRequestHeader('Password', state.dirPasswords[state.path]);
      xhr.upload.onprogress = e => { if (e.lengthComputable) bar.style.width = Math.round(e.loaded * 100 / e.total) + '%'; };
      xhr.onload = () => resolve();
      xhr.onerror = () => resolve();
      xhr.send(f);
    });
  }
  stBox.style.display = 'none';
  document.getElementById('fileInput').value = '';
  toast('✅ Upload complete');
  loadPath(state.path, true);
}

async function promptMkdir() {
  const name = (await askText('Create folder', 'Folder name')).trim();
  if (!name) return;
  const r = await api('/api/fs/mkdir', {method:'POST', json:{path: joinPath(state.path, name)}});
  toast(r.code === 200 ? '✅ Folder created' : ('❌ ' + r.message));
  if (r.code === 200) loadPath(state.path, true);
}
async function promptRename(fullPath, oldName) {
  const name = (await askText('Rename item', 'New name', oldName)).trim();
  if (!name || name === oldName) return;
  const r = await api('/api/fs/rename', {method:'POST', json:{path: fullPath, name}});
  toast(r.code === 200 ? '✅ Renamed' : ('❌ ' + r.message));
  if (r.code === 200) loadPath(state.path, true);
}
async function confirmDelete(fullPath, name) {
  if (!await askConfirm('Delete item', `Delete "${name}" permanently?`, 'Delete')) return;
  const r = await api('/api/fs/remove', {method:'POST', json:{dir: state.path, names:[name]}});
  toast(r.code === 200 ? '🗑 Deleted' : ('❌ ' + r.message));
  if (r.code === 200) loadPath(state.path, true);
}
async function promptOfflineDl() {
  const url = (await askText('Queue offline download', 'HTTP or HTTPS URL', '', 'https://example.com/file')).trim();
  if (!url) return;
  const r = await api('/api/fs/add_offline_download', {method:'POST', json:{path: state.path, urls:[url]}});
  toast(r.code === 200 ? '⚡ Offline download task queued!' : ('❌ ' + r.message));
}

function openSearch() {
  openModal(`<h3>🔍 Instant Full-Text Search (SQLite FTS5)</h3>
    <div style="display:flex;gap:8px;margin-bottom:14px">
      <input class="input" id="srchIn" placeholder="Type keywords (diacritic-insensitive)..." onkeydown="if(event.key==='Enter')runSearch()">
      <button class="btn btn-primary" onclick="runSearch()">Search</button>
    </div>
    <div id="srchRes" style="max-height:380px;overflow:auto"></div>`);
  setTimeout(() => document.getElementById('srchIn')?.focus(), 50);
}
async function runSearch() {
  const keywords = document.getElementById('srchIn').value.trim();
  if (!keywords) return;
  const r = await api('/api/fs/search', {method:'POST', json:{parent: state.path, keywords, scope:0, page:1, per_page:50}});
  const box = document.getElementById('srchRes');
  if (r.code !== 200 || !r.data?.content?.length) {
    box.innerHTML = `<div class="empty">No matching files found.</div>`;
    return;
  }
  box.innerHTML = r.data.content.map(h => {
    const p = joinPath(h.parent, h.name);
    return `<div style="padding:9px 12px;border-bottom:1px solid var(--border);cursor:pointer;display:flex;justify-content:space-between" onclick="closeModal();navTo('${esc(h.is_dir ? p : h.parent)}')">
      <span>${h.is_dir?'📁':'📄'} <b>${esc(h.name)}</b> <small style="color:var(--muted)">(${esc(h.parent)})</small></span>
      <span style="color:var(--muted)">${fmtSize(h.size, h.is_dir)}</span>
    </div>`;
  }).join('');
}

function openCreateShare(path, isDir) {
  openModal(`<h3>🔗 Create Public Share Link</h3>
    <p style="font-size:.84rem;color:var(--muted);margin-bottom:12px">Target: <code>${esc(path)}</code></p>
    <div class="grid-2">
      <div class="field"><label>Optional Share Password</label><input class="input" id="shPass" placeholder="Leave blank for open link"></div>
      <div class="field"><label>Expires In (Hours, 0 = Never)</label><input class="input" type="number" id="shExp" value="0"></div>
    </div>
    <div class="field"><label>Max Downloads (0 = Unlimited)</label><input class="input" type="number" id="shMax" value="0"></div>
    <div style="display:flex;gap:8px;justify-content:flex-end;margin-top:14px">
      <button class="btn" onclick="closeModal()">Cancel</button>
      <button class="btn btn-primary" onclick="submitCreateShare('${esc(path)}',${isDir})">Create Share</button>
    </div>`);
}
async function submitCreateShare(path, is_dir) {
  const password = document.getElementById('shPass').value;
  const expH = Number(document.getElementById('shExp').value || 0);
  const max_downloads = Number(document.getElementById('shMax').value || 0);
  const r = await api('/api/share/create', {method:'POST', json:{path, is_dir, password, expires_hours: expH, max_downloads}});
  if (r.code !== 200) { toast('❌ ' + r.message); return; }
  const url = location.origin + '/s/' + r.data.share_id;
  openModal(`<h3>✅ Share Link Ready</h3>
    <div class="field"><label>Public URL</label><input class="input" readonly value="${esc(url)}" onclick="this.select()"></div>
    <div style="display:flex;gap:8px;justify-content:flex-end">
      <button class="btn btn-primary" onclick="navigator.clipboard?.writeText('${esc(url)}');toast('Copied!')">📋 Copy URL</button>
      <button class="btn" onclick="closeModal()">Done</button>
    </div>`);
}

// PUBLIC SHARE VIEWER
async function loadPublicShare(sub) {
  state.shareSub = sub || '/';
  const info = await api('/api/public/share/info?share_id=' + encodeURIComponent(state.shareId));
  const body = document.getElementById('shareBody');
  if (info.code !== 200) { body.innerHTML = `<div class="empty">❌ ${esc(info.message)}</div>`; return; }
  if (info.data.has_password && !state.shareToken) {
    body.innerHTML = `<div class="empty"><p style="margin-bottom:12px">🔒 Password-protected share</p>
      <div style="max-width:300px;margin:0 auto;display:flex;gap:8px">
        <input class="input" type="password" id="shAuthPw" placeholder="Enter share password">
        <button class="btn btn-primary" onclick="authPublicShare()">Unlock</button>
      </div></div>`;
    return;
  }
  document.getElementById('shareMetaBadge').textContent = 'Shared by ' + info.data.creator;
  if (!info.data.is_dir) {
    const dl = `/sd/${encodeURIComponent(state.shareId)}?token=${encodeURIComponent(state.shareToken)}`;
    body.innerHTML = `<div class="empty"><h3 style="margin-bottom:10px">📄 ${esc(info.data.name)}</h3>
      <a class="btn btn-primary" href="${dl}">⬇ Download Shared File</a></div>`;
    return;
  }
  const ls = await api('/api/public/share/list', {method:'POST', headers:{'X-Share-Token':state.shareToken}, json:{share_id:state.shareId, path:state.shareSub}});
  if (ls.code !== 200) { body.innerHTML = `<div class="empty">❌ ${esc(ls.message)}</div>`; return; }
  body.innerHTML = `<table><thead><tr><th>Name</th><th>Size</th><th style="text-align:right">Download</th></tr></thead><tbody>` +
    (ls.data.content || []).map(e => {
      const sp = joinPath(state.shareSub, e.name);
      return `<tr onclick="${e.is_dir ? `loadPublicShare('${esc(sp)}')` : ''}">
        <td><div class="fname"><span class="ficon">${iconFor(e)}</span><span>${esc(e.name)}</span></div></td>
        <td>${fmtSize(e.size, e.is_dir)}</td>
        <td style="text-align:right">${!e.is_dir ? `<a class="btn" href="/sd/${encodeURIComponent(state.shareId)}${encodeURI(sp)}?token=${encodeURIComponent(state.shareToken)}">⬇</a>` : ''}</td>
      </tr>`;
    }).join('') + `</tbody></table>`;
}
async function authPublicShare() {
  const password = document.getElementById('shAuthPw').value;
  const r = await api('/api/public/share/auth', {method:'POST', json:{share_id: state.shareId, password}});
  if (r.code !== 200) { toast('❌ ' + r.message); return; }
  state.shareToken = r.data.token;
  sessionStorage.setItem('share_tok', state.shareToken);
  loadPublicShare(state.shareSub);
}

// ADMIN CONSOLE
async function toggleAdminView() {
  state.adminOpen = !state.adminOpen;
  document.getElementById('explorerView').style.display = state.adminOpen ? 'none' : 'block';
  document.getElementById('adminView').style.display = state.adminOpen ? 'block' : 'none';
  if (state.adminOpen) {
    if (!state.drivers.length) {
      const dr = await api('/api/admin/driver/list');
      if (dr.code === 200) state.drivers = dr.data || [];
    }
    loadAdminStorages();
  } else {
    loadPath(state.path, true);
  }
}
function switchAdminTab(tab, el) {
  document.querySelectorAll('.tabs .tab').forEach(t => t.classList.remove('active'));
  if (el) el.classList.add('active');
  if (tab === 'storages') loadAdminStorages();
  else if (tab === 'users') loadAdminUsers();
  else if (tab === 'roles') loadAdminRoles();
  else if (tab === 'metas') loadAdminMetas();
  else if (tab === 'shares') loadAdminShares();
  else if (tab === 'tasks') loadAdminTasks();
  else if (tab === 'settings') loadAdminSettings();
}

async function loadAdminStorages() {
  const r = await api('/api/admin/storage/list');
  const items = r.data?.content || [];
  const p = document.getElementById('adminPanelBody');
  p.innerHTML = `<div style="display:flex;justify-content:space-between;margin-bottom:14px">
    <h3>Mounted Storages (${items.length})</h3>
    <button class="btn btn-primary" onclick="openStorageModal()">+ Mount Storage</button>
  </div><table><thead><tr><th>Mount Path</th><th>Driver</th><th>Status</th><th style="text-align:right">Actions</th></tr></thead><tbody>` +
  items.map(s => `<tr>
    <td><b>${esc(s.mount_path)}</b></td>
    <td><span class="badge">${esc(s.driver)}</span></td>
    <td style="color:${s.status==='work'?'var(--ok)':'var(--warn)'}">${esc(s.status)}</td>
    <td style="text-align:right"><div class="row-ops">
      <button class="btn btn-danger" onclick="delStorage(${s.id})">Delete</button>
    </div></td>
  </tr>`).join('') + `</tbody></table>`;
}

function openStorageModal() {
  const opts = state.drivers.map(d => `<option value="${esc(d.name)}">${esc(d.display_name || d.name)}</option>`).join('');
  openModal(`<h3>Mount New Storage</h3>
    <div class="grid-2">
      <div class="field"><label>Mount Path</label><input class="input" id="stMount" placeholder="/local or /s3"></div>
      <div class="field"><label>Driver</label><select class="input" id="stDrv" onchange="renderDriverFields()">${opts}</select></div>
    </div>
    <div class="grid-2">
      <div class="field"><label>Cache TTL (minutes)</label><input class="input" type="number" id="stCache" value="30"></div>
      <div class="field"><label>HMAC Sign Links</label><select class="input" id="stSign"><option value="false">No</option><option value="true">Yes (Signed URLs)</option></select></div>
    </div>
    <hr style="border-color:var(--border);margin:12px 0">
    <div id="drvFields"></div>
    <div style="display:flex;gap:8px;justify-content:flex-end;margin-top:14px">
      <button class="btn" onclick="closeModal()">Cancel</button>
      <button class="btn btn-primary" onclick="saveStorage()">Mount</button>
    </div>`);
  renderDriverFields();
}

function renderDriverFields() {
  const name = document.getElementById('stDrv').value;
  const drv = state.drivers.find(d => d.name === name);
  const box = document.getElementById('drvFields');
  if (!drv) { box.innerHTML = ''; return; }
  box.innerHTML = (drv.fields || []).map(f => {
    let ctrl = '';
    if (f.type === 'bool') {
      ctrl = `<select class="input" data-df="${esc(f.name)}" data-dt="bool"><option value="false" ${f.default_val==='false'?'selected':''}>false</option><option value="true" ${f.default_val==='true'?'selected':''}>true</option></select>`;
    } else if (f.type === 'text') {
      ctrl = `<textarea class="input" rows="4" data-df="${esc(f.name)}">${esc(f.default_val)}</textarea>`;
    } else {
      ctrl = `<input class="input" type="${f.type==='password'?'password':(f.type==='number'?'number':'text')}" data-df="${esc(f.name)}" data-dt="${esc(f.type)}" value="${esc(f.default_val)}" placeholder="${esc(f.help_text)}">`;
    }
    return `<div class="field"><label>${esc(f.label)} ${f.required?'*':''}</label>${ctrl}</div>`;
  }).join('');
}

async function saveStorage() {
  const mount_path = document.getElementById('stMount').value.trim();
  const driver = document.getElementById('stDrv').value;
  const cache_expiration = Number(document.getElementById('stCache').value || 30);
  const enable_sign = document.getElementById('stSign').value === 'true';
  const add = {};
  document.querySelectorAll('#drvFields [data-df]').forEach(el => {
    const k = el.getAttribute('data-df'), t = el.getAttribute('data-dt');
    add[k] = t === 'bool' ? (el.value === 'true') : (t === 'number' ? Number(el.value || 0) : el.value);
  });
  const r = await api('/api/admin/storage/create', {method:'POST', json:{mount_path, driver, cache_expiration, enable_sign, addition: JSON.stringify(add)}});
  if (r.code !== 200) { toast('❌ ' + r.message); return; }
  closeModal();
  toast('✅ Storage mounted!');
  loadAdminStorages();
}

async function delStorage(id) {
  if (!await askConfirm('Remove storage', 'Unmount and delete this storage configuration?', 'Remove storage')) return;
  await api('/api/admin/storage/delete?id=' + id, {method:'POST'});
  loadAdminStorages();
}

async function loadAdminUsers() {
  const r = await api('/api/admin/user/list');
  const items = r.data?.content || [];
  document.getElementById('adminPanelBody').innerHTML = `<div style="display:flex;justify-content:space-between;margin-bottom:14px">
    <h3>Users (${items.length})</h3>
    <button class="btn btn-primary" onclick="openUserModal()">+ Create User</button>
  </div><table><thead><tr><th>Username</th><th>Identity</th><th>Base Path</th><th>Status</th><th style="text-align:right">Actions</th></tr></thead><tbody>` +
  items.map(u => `<tr>
    <td><b>${esc(u.username)}</b></td>
    <td>${u.role===2?'Admin':(u.role===1?'Guest':'User')}</td>
    <td><code>${esc(u.base_path)}</code></td>
    <td>${u.disabled?'Disabled':'Active'}</td>
    <td style="text-align:right"><div class="row-ops">
      ${u.role===1 ? `<button class="btn" onclick="toggleGuest(${u.id},${!u.disabled})">${u.disabled?'Enable Guest':'Disable Guest'}</button>` : ''}
      ${u.role===0 ? `<button class="btn btn-danger" onclick="api('/api/admin/user/delete?id=${u.id}',{method:'POST'}).then(loadAdminUsers)">Delete</button>` : ''}
    </div></td>
  </tr>`).join('') + `</tbody></table>`;
}

async function toggleGuest(id, disabled) {
  await api('/api/admin/user/update', {method:'POST', json:{id, username:'guest', role:1, base_path:'/', disabled}});
  loadAdminUsers();
}

function openUserModal() {
  openModal(`<h3>Create User</h3>
    <div class="field"><label>Username</label><input class="input" id="nuName"></div>
    <div class="field"><label>Password</label><input class="input" type="password" id="nuPass"></div>
    <div class="field"><label>Base Path Confinement</label><input class="input" id="nuBase" value="/"></div>
    <div style="display:flex;gap:8px;justify-content:flex-end;margin-top:14px">
      <button class="btn" onclick="closeModal()">Cancel</button>
      <button class="btn btn-primary" onclick="saveNewUser()">Create</button>
    </div>`);
}
async function saveNewUser() {
  const r = await api('/api/admin/user/create', {method:'POST', json:{
    username: document.getElementById('nuName').value.trim(),
    password: document.getElementById('nuPass').value,
    base_path: document.getElementById('nuBase').value.trim() || '/',
    role_ids: [2]
  }});
  if (r.code !== 200) { toast('❌ ' + r.message); return; }
  closeModal(); loadAdminUsers();
}

async function loadAdminRoles() {
  const r = await api('/api/admin/role/list');
  const items = r.data?.content || [];
  document.getElementById('adminPanelBody').innerHTML = `<h3 style="margin-bottom:14px">RBAC Roles (${items.length})</h3>
    <table><thead><tr><th>ID</th><th>Name</th><th>Description</th><th>Scopes</th></tr></thead><tbody>` +
    items.map(ro => `<tr><td>${ro.id}</td><td><b>${esc(ro.name)}</b></td><td>${esc(ro.description)}</td><td><code>${esc(JSON.stringify(ro.permission_scopes))}</code></td></tr>`).join('') +
    `</tbody></table>`;
}

async function loadAdminMetas() {
  const r = await api('/api/admin/meta/list');
  const items = r.data?.content || [];
  document.getElementById('adminPanelBody').innerHTML = `<div style="display:flex;justify-content:space-between;margin-bottom:14px">
    <h3>Path Metas (${items.length})</h3>
    <button class="btn btn-primary" onclick="openMetaModal()">+ Add Meta Rule</button>
  </div><table><thead><tr><th>Path</th><th>Password</th><th>Hide Globs</th><th style="text-align:right">Actions</th></tr></thead><tbody>` +
  items.map(m => `<tr><td><code>${esc(m.path)}</code></td><td>${m.password?'Protected':'-'}</td><td><code>${esc(m.hide)}</code></td>
    <td style="text-align:right"><button class="btn btn-danger" onclick="api('/api/admin/meta/delete?id=${m.id}',{method:'POST'}).then(loadAdminMetas)">Delete</button></td></tr>`).join('') +
  `</tbody></table>`;
}
function openMetaModal() {
  openModal(`<h3>Add Path Meta Rule</h3>
    <div class="field"><label>VFS Path</label><input class="input" id="mtPath" value="/local"></div>
    <div class="field"><label>Folder Password</label><input class="input" id="mtPass"></div>
    <div class="field"><label>Hide Patterns (newline separated globs, e.g. *.secret)</label><textarea class="input" id="mtHide"></textarea></div>
    <div class="field"><label>README Text</label><textarea class="input" id="mtReadme"></textarea></div>
    <div style="display:flex;gap:8px;justify-content:flex-end;margin-top:14px">
      <button class="btn" onclick="closeModal()">Cancel</button>
      <button class="btn btn-primary" onclick="saveMeta()">Save</button>
    </div>`);
}
async function saveMeta() {
  const r = await api('/api/admin/meta/create', {method:'POST', json:{
    path: document.getElementById('mtPath').value.trim(),
    password: document.getElementById('mtPass').value,
    p_sub: true,
    hide: document.getElementById('mtHide').value,
    h_sub: true,
    readme: document.getElementById('mtReadme').value
  }});
  if (r.code !== 200) { toast('❌ ' + r.message); return; }
  closeModal(); loadAdminMetas();
}

async function loadAdminShares() {
  const r = await api('/api/share/list');
  const items = r.data?.content || [];
  document.getElementById('adminPanelBody').innerHTML = `<h3 style="margin-bottom:14px">Public Shares (${items.length})</h3>
    <table><thead><tr><th>Share ID</th><th>Path</th><th>Downloads</th><th style="text-align:right">Actions</th></tr></thead><tbody>` +
    items.map(s => `<tr><td><a href="/s/${esc(s.share_id)}" target="_blank"><code>${esc(s.share_id)}</code></a></td>
      <td><code>${esc(s.path)}</code></td><td>${s.downloads}${s.max_downloads?'/'+s.max_downloads:''}</td>
      <td style="text-align:right"><button class="btn btn-danger" onclick="api('/api/share/delete',{method:'POST',json:{share_id:'${esc(s.share_id)}'}}).then(loadAdminShares)">Revoke</button></td></tr>`).join('') +
    `</tbody></table>`;
}

async function loadAdminTasks() {
  const u = await api('/api/task/undone');
  const d = await api('/api/task/done');
  const all = [...(u.data||[]), ...(d.data||[])];
  document.getElementById('adminPanelBody').innerHTML = `<div style="display:flex;justify-content:space-between;margin-bottom:14px">
    <h3>Background Tasks (${all.length})</h3>
    <div style="display:flex;gap:8px">
      <button class="btn" onclick="loadAdminTasks()">↻ Refresh</button>
      <button class="btn" onclick="api('/api/task/clear_done',{method:'POST'}).then(loadAdminTasks)">Clear Completed</button>
    </div>
  </div><table><thead><tr><th>ID</th><th>Name</th><th>State</th><th>Progress</th></tr></thead><tbody>` +
  all.map(t => `<tr><td><code>${esc(t.id)}</code></td><td>${esc(t.name)}</td><td>${esc(t.state)} ${t.error?('('+esc(t.error)+')'):''}</td><td>${Math.round(t.progress||0)}%</td></tr>`).join('') +
  `</tbody></table>`;
}

async function loadAdminSettings() {
  const r = await api('/api/admin/setting/list');
  const m = r.data || {};
  const idx = await api('/api/admin/index/progress');
  document.getElementById('adminPanelBody').innerHTML = `<div class="grid-2">
    <div>
      <h3 style="margin-bottom:12px">Site & Security Settings</h3>
      <div class="field"><label>Site Title</label><input class="input" id="stTitle" value="${esc(m.site_title||'AList · @nggkhaiz')}"></div>
      <div class="field"><label>Force Sign All Download URLs</label><select class="input" id="stSignAll"><option value="false" ${m.sign_all==='false'?'selected':''}>false</option><option value="true" ${m.sign_all==='true'?'selected':''}>true</option></select></div>
      <div class="field"><label>Max Active Devices Per User (0 = Unlimited)</label><input class="input" type="number" id="stMaxDev" value="${esc(m.max_devices||'0')}"></div>
      <button class="btn btn-primary" onclick="saveSettings()">Save Settings</button>
    </div>
    <div>
      <h3 style="margin-bottom:12px">SQLite FTS5 Search Index</h3>
      <p style="color:var(--muted);margin-bottom:12px">Indexed entries: <b>${idx.data?.obj_count ?? 0}</b></p>
      <button class="btn btn-primary" onclick="api('/api/admin/index/build',{method:'POST'}).then(()=>{toast('✅ Search index rebuilt!');loadAdminSettings();})">⚡ Rebuild Full Index Now</button>
    </div>
  </div>`;
}
async function saveSettings() {
  await api('/api/admin/setting/save', {method:'POST', json:[
    {key:'site_title', value: document.getElementById('stTitle').value},
    {key:'sign_all', value: document.getElementById('stSignAll').value},
    {key:'max_devices', value: document.getElementById('stMaxDev').value}
  ]});
  toast('✅ Settings saved');
  initApp();
}

initApp();
</script>
</body>
</html>
)HTML";

} // namespace alist
