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
.entry-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(180px,1fr));gap:10px;padding:14px}
.entry-tile{min-height:120px;padding:14px;border:1px solid var(--border);border-radius:10px;background:var(--panel2);display:flex;flex-direction:column;justify-content:space-between;cursor:pointer;overflow:hidden}
.entry-tile:hover{border-color:var(--accent)}
.entry-tile .tile-name{overflow-wrap:anywhere;font-weight:550;margin-top:10px}
.selection-box{accent-color:var(--accent);width:16px;height:16px;cursor:pointer}
.drop-active{outline:2px dashed var(--accent);outline-offset:-6px;background:var(--panel2)}
.ficon{width:26px;display:inline-flex;align-items:center;justify-content:center;flex-shrink:0;color:var(--muted)}
.ficon svg{width:19px;height:19px;stroke:currentColor;fill:none;stroke-width:1.7;stroke-linecap:round;stroke-linejoin:round}
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
  <div class="brand" role="button" tabindex="0" onclick="navTo('/')" onkeydown="if(event.key==='Enter')navTo('/')">
    <img class="brand-logo" alt="@nggkhaiz logo" width="30" height="30" src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAB00lEQVR4AeyVuy8EURTG1zMKBQUaJJ4FohcKBVqPQqGgEDoqQkFIaLwaOioaQYFWNBL8AUgIiZKIaISIxOt3kh13xtyxszP7Kmbz/XLuuffMmW/vzN5NDyX5ExhIyR345rWIJ7RXSskdUPYSMAp2INgB3Q6k8e55pYZrD0GnSyZbwSKdAUuBy6SAulU4hxYw64FkEOrAZs6vgRyaTsAtDEAGGHpjMAsVsAZfYJNXA/KIeul2AzOQC4bkFN0gqYRJeAVHeTHQSLczWIdiMOuEpB764A4iKhoDspV7dDwGeZ6EX10z6oAmuADXcmMgn27LcAXtYNYTyTDUwj5Erf8MZNNtBOQFGyJmgqF3BotQDivwAZ7kZKCbbvKNF4h5YNY2SRWMwjP4ks7AKR23oAz+6oAJOVD6idMe4TIlnYEGtWwbtTEz5RMuV9IZUKsJGAUGnHZAflabPIFOkJ+fHL1CFnkXyNon0bd0BpboWgo9ICef+UZibJd5WSshSu0L0a1stToDcvjcu+goNVJbTa0YeSQ6SdakRmotNToDlgIXiWGkiFo5wOaJR2HmiDJXSBSzUstQKRYGjG7yN7xDMgbNYcaJMkfQK5YG9HeIMBt3AxHuH/oBAAD//zKEaE0AAAAGSURBVAMAMcNrQerOH9kAAAAASUVORK5CYII=">
    <span id="siteTitle">AList @nggkhaiz</span>
    <span class="badge">v4.1.0</span>
  </div>
  <div class="actions">
    <button class="btn" onclick="openSearch()">Search</button>
    <button class="btn" onclick="toggleTheme()" title="Switch theme">Theme</button>
    <span id="userBadge" style="font-size:.84rem;color:var(--muted)"></span>
    <button class="btn" id="btnSecurity" style="display:none" onclick="openSecurityModal()">Security</button>
    <button class="btn" id="btnAdmin" style="display:none" onclick="toggleAdminView()">Admin</button>
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
          <button class="btn" onclick="setViewMode('list')" aria-label="List view">List</button>
          <button class="btn" onclick="setViewMode('grid')" aria-label="Grid view">Grid</button>
          <button class="btn" onclick="loadPath(state.path, true)">Refresh</button>
        </div>
      </div>
      <div class="toolbar" id="writeToolbar" style="display:none">
        <label class="btn btn-primary" style="cursor:pointer">
          Upload files
          <input type="file" id="fileInput" multiple style="display:none" onchange="uploadFiles(this.files)">
        </label>
        <button class="btn" onclick="promptMkdir()">New folder</button>
        <button class="btn" onclick="promptOfflineDl()">Offline URL</button>
        <div id="batchBar" style="display:none;align-items:center;gap:6px">
          <span id="selectedCount" style="color:var(--muted)"></span>
          <button class="btn" onclick="batchTransfer('copy')">Copy to</button>
          <button class="btn" onclick="batchTransfer('move')">Move to</button>
          <button class="btn btn-danger" onclick="batchDelete()">Delete selected</button>
          <button class="btn" onclick="clearSelection()">Clear</button>
        </div>
        <div id="uploadStatus" style="flex:1;min-width:180px;display:none">
          <div style="font-size:.78rem;color:var(--muted)" id="uploadLabel">Uploading...</div>
          <div class="prog"><div id="uploadBar"></div></div>
          <button class="btn" id="uploadCancel" onclick="cancelUpload()">Cancel queue</button>
        </div>
      </div>
      <div id="explorerContent"></div>
    </div>
    <div class="card" id="readmeCard" style="display:none">
      <div class="crumbs"><strong>README</strong></div>
      <div class="readme-card" id="readmeBody"></div>
    </div>
  </section>

  <!-- ADMIN CONSOLE VIEW -->
  <section id="adminView" style="display:none">
    <div class="card">
      <div class="tabs">
        <div class="tab active" onclick="switchAdminTab('storages',this)">Storages</div>
        <div class="tab" onclick="switchAdminTab('users',this)">Users</div>
        <div class="tab" onclick="switchAdminTab('roles',this)">Roles</div>
        <div class="tab" onclick="switchAdminTab('metas',this)">Path metas</div>
        <div class="tab" onclick="switchAdminTab('shares',this)">Shares</div>
        <div class="tab" onclick="switchAdminTab('tasks',this)">Tasks</div>
        <div class="tab" onclick="switchAdminTab('telemetry',this)">Telemetry</div>
        <div class="tab" onclick="switchAdminTab('settings',this)">Settings & search</div>
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
  selected: new Set(),
  viewMode: localStorage.getItem('alist_view') === 'grid' ? 'grid' : 'list',
  uploadXhr: null,
  uploadCancelled: false,
  textEdit: null,
  telemetryTimer: null,
  pendingOtpSecret: '',
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
function safeArg(value) {
  return encodeURIComponent(String(value)).replace(/[!'()*]/g, c => '%' + c.charCodeAt(0).toString(16).toUpperCase());
}
function safeHref(value) {
  const s = String(value || '');
  try { const u = new URL(s, location.origin); return (u.protocol === 'http:' || u.protocol === 'https:') ? esc(s) : '#'; }
  catch { return '#'; }
}
function encodePath(path) { return String(path).split('/').map(part => encodeURIComponent(part)).join('/'); }
function fmtSize(b, isDir) {
  if (isDir) return '-';
  const u = ['B','KB','MB','GB','TB'];
  let i = 0, v = Number(b || 0);
  while (v >= 1024 && i < u.length - 1) { v /= 1024; i++; }
  return (i === 0 ? v : v.toFixed(1)) + ' ' + u[i];
}
function iconFor(e) {
  let p='<path d="M4 5.5h6l2 2h8v11H4z"/><path d="M4 9h16"/>';
  if(!e.is_dir){
    const paths={2:'<path d="M7 3.5h7l4 4v13H7z"/><path d="M14 3.5v4h4M10 12h5M10 15h5"/>',3:'<rect x="3.5" y="5" width="17" height="14" rx="2"/><path d="m10 9 5 3-5 3z"/>',4:'<path d="M9 18V5l10-2v13"/><circle cx="6" cy="18" r="3"/><circle cx="16" cy="16" r="3"/>',5:'<path d="M7 3.5h7l4 4v13H7z"/><path d="M14 3.5v4h4M10 12h5M10 15h5"/>',6:'<rect x="3.5" y="4.5" width="17" height="15" rx="2"/><circle cx="9" cy="10" r="2"/><path d="m5 17 5-4 3 2 3-4 4 6"/>',7:'<path d="M4 6.5h16v13H4zM4 10h16M8 6.5V4m8 2.5V4"/>'};p=paths[e.type]||'<path d="M7 3.5h7l4 4v13H7z"/><path d="M14 3.5v4h4"/>';
  }
  return `<svg viewBox="0 0 24 24" aria-hidden="true">${p}</svg>`;
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
    document.getElementById('userBadge').textContent = isGuest ? 'Guest' : ('' + r.data.username);
    document.getElementById('btnAuth').textContent = isGuest ? 'Sign In' : 'Sign Out';
    document.getElementById('btnAuth').onclick = isGuest ? openLogin : doLogout;
    document.getElementById('btnAdmin').style.display = (r.data.role === 2) ? 'inline-flex' : 'none';
    document.getElementById('btnSecurity').style.display = isGuest ? 'none' : 'inline-flex';
  } else {
    state.me = null;
    document.getElementById('userBadge').textContent = '';
    document.getElementById('btnAuth').textContent = 'Sign In';
    document.getElementById('btnAuth').onclick = openLogin;
    document.getElementById('btnAdmin').style.display = 'none';
    document.getElementById('btnSecurity').style.display = 'none';
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
  if (r.code !== 200) { toast('Error:  ' + (r.message || 'Login failed')); return; }
  state.token = r.data.token;
  localStorage.setItem('alist_token', state.token);
  closeModal();
  toast(' Signed in as ' + username);
  await refreshMe();
  await loadPath(state.path, true);
}

async function openSecurityModal() {
  const otpEnabled=!!state.me?.otp;
  if(otpEnabled){openModal(`<h3>Account security</h3><p style="margin-bottom:12px;color:var(--muted)">Authenticator-based two-factor authentication is enabled.</p><label class="field"><span>Current 6-digit code</span><input class="input" id="otpDisableCode" inputmode="numeric" autocomplete="one-time-code" maxlength="6"></label><div style="display:flex;justify-content:flex-end;gap:8px"><button class="btn" onclick="closeModal()">Close</button><button class="btn btn-danger" onclick="disableTwoFactor()">Disable 2FA</button></div>`);return;}
  const g=await api('/api/auth/2fa/generate',{method:'POST',json:{}});
  if(g.code!==200){toast(g.message||'Could not start 2FA setup');return;}
  state.pendingOtpSecret=g.data.secret;
  openModal(`<h3>Set up two-factor authentication</h3><p style="color:var(--muted);margin-bottom:10px">Add this key to an authenticator app, then enter the current 6-digit code to verify.</p><div class="field"><label>Setup key</label><input class="input" readonly value="${esc(g.data.secret)}" onclick="this.select()"></div><div class="field"><label>Authenticator URI</label><input class="input" readonly value="${esc(g.data.qr)}" onclick="this.select()"></div><div class="field"><label>Verification code</label><input class="input" id="otpEnableCode" inputmode="numeric" autocomplete="one-time-code" maxlength="6"></div><div style="display:flex;justify-content:flex-end;gap:8px"><button class="btn" onclick="closeModal()">Cancel</button><button class="btn btn-primary" onclick="enableTwoFactor()">Verify and enable</button></div>`);
}
async function enableTwoFactor(){const code=document.getElementById('otpEnableCode').value.trim();const r=await api('/api/auth/2fa/verify',{method:'POST',json:{secret:state.pendingOtpSecret,code}});if(r.code!==200){toast(r.message||'Code verification failed');return;}state.pendingOtpSecret='';closeModal();toast('Two-factor authentication enabled');await refreshMe();}
async function disableTwoFactor(){const code=document.getElementById('otpDisableCode').value.trim();const r=await api('/api/auth/2fa/cancel',{method:'POST',json:{code}});if(r.code!==200){toast(r.message||'Could not disable 2FA');return;}closeModal();toast('Two-factor authentication disabled');await refreshMe();}

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
  let html = `<span class="crumb ${segs.length===0?'active':''}" onclick="navTo('/')">Home</span>`;
  let cur = '';
  segs.forEach((s, idx) => {
    cur += '/' + s;
    const target = cur;
    html += `<span style="color:var(--muted)">/</span><span class="crumb ${idx===segs.length-1?'active':''}" onclick="navTo(decodeURIComponent('${safeArg(target)}'))">${esc(s)}</span>`;
  });
  el.innerHTML = html;
}

async function loadPath(p, refresh) {
  const nextPath = p || '/';
  if (nextPath !== state.path) state.selected.clear();
  state.path = nextPath;
  renderBreadcrumbs(state.path);
  const pw = state.dirPasswords[state.path] || '';
  const r = await api('/api/fs/list', {method:'POST', json:{path: state.path, password: pw, refresh: !!refresh, page:1, per_page:0}});
  const box = document.getElementById('explorerContent');
  if (r.code === 401) {
    box.innerHTML = `<div class="empty"><p style="margin-bottom:12px">Sign in is required to browse files.</p><button class="btn btn-primary" onclick="openLogin()">Sign In</button></div>`;
    document.getElementById('writeToolbar').style.display = 'none';
    return;
  }
  if (r.code === 403 && (r.message || '').toLowerCase().includes('password')) {
    box.innerHTML = `<div class="empty"><p style="margin-bottom:12px">This folder is protected by a password.</p>
      <div style="max-width:300px;margin:0 auto;display:flex;gap:8px">
        <input class="input" type="password" id="dirPwIn" placeholder="Folder password">
        <button class="btn btn-primary" onclick="state.dirPasswords[state.path]=document.getElementById('dirPwIn').value;loadPath(state.path,false)">Unlock</button>
      </div></div>`;
    return;
  }
  if (r.code !== 200) {
    box.innerHTML = `<div class="empty">Notice:  ${esc(r.message || 'Unable to load directory')}</div>`;
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
  renderBatchBar();
  if (!state.entries.length) {
    box.innerHTML = `<div class="empty">Empty directory. ${state.me?.role===2 && state.path==='/' ? 'Open <b>Admin → Storages</b> to mount Local, S3, WebDAV, Cloud, or Crypt storage.' : ''}</div>`;
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
  if (state.viewMode === 'grid') {
    box.innerHTML = `<div class="entry-grid">` + list.map(e =>
      `<div class="entry-tile" role="button" tabindex="0" onclick="openEntry(decodeURIComponent('${safeArg(e.name)}'),${!!e.is_dir})" onkeydown="if(event.key==='Enter')openEntry(decodeURIComponent('${safeArg(e.name)}'),${!!e.is_dir})">
        <div style="display:flex;align-items:center;justify-content:space-between"><span class="ficon">${iconFor(e)}</span><input class="selection-box" type="checkbox" aria-label="Select ${esc(e.name)}" ${state.selected.has(e.name)?'checked':''} onclick="event.stopPropagation()" onchange="toggleSelection(decodeURIComponent('${safeArg(e.name)}'),this.checked)"></div>
        <div class="tile-name">${esc(e.name)}</div><small style="color:var(--muted)">${fmtSize(e.size,e.is_dir)}</small>
      </div>`).join('') + `</div>`;
    return;
  }
  let rows = list.map(e => {
    const full = joinPath(state.path, e.name);
    const safeName = safeArg(e.name), safeFull = safeArg(full);
    const dlUrl = '/d' + encodePath(full) + (e.sign ? ('?sign=' + encodeURIComponent(e.sign)) : '');
    return `<tr onclick="openEntry(decodeURIComponent('${safeName}'), ${!!e.is_dir})">
      <td><input class="selection-box" type="checkbox" aria-label="Select ${esc(e.name)}" ${state.selected.has(e.name)?'checked':''} onclick="event.stopPropagation()" onchange="toggleSelection(decodeURIComponent('${safeName}'),this.checked)"><div class="fname"><span class="ficon">${iconFor(e)}</span><span>${esc(e.name)}</span></div></td>
      <td style="width:120px;color:var(--muted)">${fmtSize(e.size, e.is_dir)}</td>
      <td style="width:190px;color:var(--muted)">${esc((e.modified||'').replace('T',' ').replace('Z',''))}</td>
      <td style="width:210px" onclick="event.stopPropagation()"><div class="row-ops">
        ${!e.is_dir ? `<a class="btn" href="${dlUrl}" download>⬇</a>` : ''}
        ${canManage ? `<button class="btn" title="Share" onclick="openCreateShare(decodeURIComponent('${safeFull}'),${!!e.is_dir})">Share</button>
        <button class="btn" title="Rename" onclick="promptRename(decodeURIComponent('${safeFull}'),decodeURIComponent('${safeName}'))">Rename</button>
        <button class="btn btn-danger" title="Delete" onclick="confirmDelete(decodeURIComponent('${safeFull}'),decodeURIComponent('${safeName}'))">Delete</button>` : ''}
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

function setViewMode(mode) {
  state.viewMode = mode === 'grid' ? 'grid' : 'list';
  localStorage.setItem('alist_view', state.viewMode);
  renderEntries();
}
function toggleSelection(name, checked) {
  checked ? state.selected.add(name) : state.selected.delete(name);
  renderBatchBar();
}
function clearSelection() { state.selected.clear(); renderEntries(); }
function renderBatchBar() {
  const bar = document.getElementById('batchBar');
  if (!bar) return;
  bar.style.display = state.selected.size ? 'flex' : 'none';
  const count = document.getElementById('selectedCount');
  if (count) count.textContent = `${state.selected.size} selected`;
}
async function batchDelete() {
  const names = [...state.selected];
  if (!names.length || !await askConfirm('Delete selected items', `Permanently delete ${names.length} selected item(s)?`, 'Delete')) return;
  const r = await api('/api/fs/remove', {method:'POST', json:{dir:state.path,names}});
  if (r.code !== 200) { toast('Delete failed: ' + (r.message || 'request failed')); return; }
  state.selected.clear(); toast('Selected items deleted'); loadPath(state.path, true);
}
async function batchTransfer(kind) {
  const names = [...state.selected];
  if (!names.length) return;
  const dst_dir = (await askText(kind === 'move' ? 'Move selected items' : 'Copy selected items', 'Destination folder path', state.path)).trim();
  if (!dst_dir) return;
  const r = await api('/api/fs/' + kind, {method:'POST', json:{src_dir:state.path,dst_dir,names,as_task:kind==='copy'}});
  if (r.code !== 200) { toast((kind==='move'?'Move':'Copy') + ' failed: ' + (r.message || 'request failed')); return; }
  state.selected.clear(); toast(`${kind==='move'?'Move':'Copy'} queued`); loadPath(state.path, true);
}

async function openEntry(name, isDir) {
  const full = joinPath(state.path, name);
  if (isDir) { navTo(full); return; }
  const pw = state.dirPasswords[state.path] || '';
  const r = await api('/api/fs/get', {method:'POST', json:{path: full, password: pw}});
  if (r.code !== 200) { toast('Error:  ' + r.message); return; }
  const d = r.data;
  const rawUrl = d.raw_url || ('/d' + encodePath(full) + (d.sign ? '?sign=' + encodeURIComponent(d.sign) : ''));
  const proxyUrl = '/p' + encodePath(full) + (d.sign ? '?sign=' + encodeURIComponent(d.sign) : '');
  let body = '';
  if (d.type === 3) {
    body = `<video controls autoplay src="${proxyUrl}"></video>`;
  } else if (d.type === 4) {
    body = `<audio controls autoplay src="${proxyUrl}" style="width:100%"></audio>`;
  } else if (d.type === 6) {
    body = `<img src="${proxyUrl}" alt="${esc(d.name)}">`;
  } else if (d.type === 5 && d.size <= 524288) {
    const txt = await (await fetch(proxyUrl)).text();
    state.textEdit = {path:full,text:txt};
    body = `<pre class="code-view">${esc(txt)}</pre>`;
  } else if (name.toLowerCase().endsWith('.zip')) {
    const ar = await api('/api/fs/archive/list', {method:'POST', json:{path: full, password: pw}});
    if (ar.code === 200 && ar.data?.content) {
      body = `<div style="margin-bottom:8px;color:var(--muted)">ZIP archive contents (${ar.data.content.length} items):</div>
        <div style="max-height:360px;overflow:auto;border:1px solid var(--border);border-radius:8px">` +
        ar.data.content.map(z => `<div style="display:flex;justify-content:space-between;padding:8px 12px;border-bottom:1px solid var(--border);font-size:.85rem">
          <span>${z.is_dir?'Folder ':'File '} ${esc(z.path)}</span>
          <span>${fmtSize(z.size, z.is_dir)} ${!z.is_dir ? `<a class="btn" style="padding:2px 8px;margin-left:8px" href="/ad${encodePath(full)}?inner=${encodeURIComponent(z.path)}&sign=${encodeURIComponent(z.sign||'')}">Extract</a>` : ''}</span>
        </div>`).join('') + `</div>`;
    }
  } else {
    body = `<p style="color:var(--muted);padding:16px 0">File size: <b>${fmtSize(d.size,false)}</b> · Provider: <b>${esc(d.provider)}</b></p>`;
  }
  openModal(`<h3>${iconFor(d)} ${esc(d.name)}</h3>
    <div class="preview-box">${body}</div>
    <div style="display:flex;gap:8px;justify-content:flex-end;margin-top:14px">
      ${d.type===5 && state.textEdit && state.me && state.me.role!==1 ? `<button class="btn" onclick="openTextEditor()">Edit text</button>` : ''}
      <button class="btn" onclick="navigator.clipboard?.writeText(location.origin+decodeURIComponent('${safeArg(rawUrl)}'));toast('Copied direct link!')">Copy link</button>
      <a class="btn btn-primary" href="${safeHref(rawUrl)}" download>⬇ Download</a>
      <button class="btn" onclick="closeModal()">Close</button>
    </div>`);
}

function openTextEditor() {
  if (!state.textEdit) return;
  openModal(`<h3>Text editor · ${esc(state.textEdit.path.split('/').pop())}</h3>
    <textarea id="textEditor" class="input" spellcheck="false" style="min-height:55vh;font-family:ui-monospace,monospace;resize:vertical">${esc(state.textEdit.text)}</textarea>
    <div style="display:flex;justify-content:space-between;align-items:center;margin-top:12px"><small style="color:var(--muted)">Ctrl/⌘+S to save</small><div style="display:flex;gap:8px"><button class="btn" onclick="closeModal()">Cancel</button><button class="btn btn-primary" onclick="saveTextEditor()">Save</button></div></div>`);
  document.getElementById('textEditor').addEventListener('keydown', event => {
    if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 's') { event.preventDefault(); saveTextEditor(); }
    if (event.key === 'Tab') { event.preventDefault(); const el = event.target; const a=el.selectionStart,b=el.selectionEnd; el.setRangeText('  ',a,b,'end'); }
  });
}
async function saveTextEditor() {
  if (!state.textEdit) return;
  const text = document.getElementById('textEditor')?.value;
  if (text === undefined) return;
  const r = await api('/api/fs/put', {method:'PUT',headers:{'File-Path':encodeURIComponent(state.textEdit.path),'Password':state.dirPasswords[state.path]||''},body:text});
  if (r.code !== 200) { toast('Save failed: ' + (r.message || 'request failed')); return; }
  state.textEdit.text = text;
  toast('File saved');
  closeModal();
  loadPath(state.path, true);
}

function cancelUpload() {
  state.uploadCancelled = true;
  state.uploadXhr?.abort();
}
async function uploadFiles(files) {
  if (!files || !files.length || state.uploadXhr) return;
  state.uploadCancelled = false;
  const stBox = document.getElementById('uploadStatus');
  const bar = document.getElementById('uploadBar');
  const lbl = document.getElementById('uploadLabel');
  stBox.style.display = 'block';
  for (let i = 0; i < files.length; i++) {
    if (state.uploadCancelled) break;
    const f = files[i];
    const target = joinPath(state.path, f.name);
    lbl.textContent = `Uploading (${i+1}/${files.length}): ${f.name}`;
    bar.style.width = '0%';
    await new Promise((resolve) => {
      const xhr = new XMLHttpRequest();
      state.uploadXhr = xhr;
      const started = performance.now();
      xhr.open('PUT', '/api/fs/put');
      if (state.token) xhr.setRequestHeader('Authorization', state.token);
      xhr.setRequestHeader('Client-Id', state.clientId);
      xhr.setRequestHeader('File-Path', encodeURIComponent(target));
      if (state.dirPasswords[state.path]) xhr.setRequestHeader('Password', state.dirPasswords[state.path]);
      xhr.upload.onprogress = e => {
        if (!e.lengthComputable) return;
        bar.style.width = Math.round(e.loaded * 100 / e.total) + '%';
        const elapsed = Math.max(0.001, (performance.now() - started) / 1000);
        lbl.textContent = `Uploading (${i+1}/${files.length}): ${f.name} · ${(e.loaded / elapsed / 1048576).toFixed(2)} MB/s`;
      };
      xhr.onload = () => {
        let payload = null;
        try { payload = JSON.parse(xhr.responseText); } catch {}
        const ok = xhr.status >= 200 && xhr.status < 300 && payload?.code === 200;
        if (!ok) toast(`Upload failed: ${payload?.message || f.name}`);
        resolve(ok);
      };
      xhr.onerror = () => { toast(`Network error uploading ${f.name}`); resolve(false); };
      xhr.onabort = () => resolve(false);
      xhr.send(f);
    });
    state.uploadXhr = null;
  }
  state.uploadXhr = null;
  stBox.style.display = 'none';
  document.getElementById('fileInput').value = '';
  toast(state.uploadCancelled ? 'Upload queue cancelled' : 'Upload queue finished');
  loadPath(state.path, true);
}

async function promptMkdir() {
  const name = (await askText('Create folder', 'Folder name')).trim();
  if (!name) return;
  const r = await api('/api/fs/mkdir', {method:'POST', json:{path: joinPath(state.path, name)}});
  toast(r.code === 200 ? ' Folder created' : ('Error:  ' + r.message));
  if (r.code === 200) loadPath(state.path, true);
}
async function promptRename(fullPath, oldName) {
  const name = (await askText('Rename item', 'New name', oldName)).trim();
  if (!name || name === oldName) return;
  const r = await api('/api/fs/rename', {method:'POST', json:{path: fullPath, name}});
  toast(r.code === 200 ? ' Renamed' : ('Error:  ' + r.message));
  if (r.code === 200) loadPath(state.path, true);
}
async function confirmDelete(fullPath, name) {
  if (!await askConfirm('Delete item', `Delete "${name}" permanently?`, 'Delete')) return;
  const r = await api('/api/fs/remove', {method:'POST', json:{dir: state.path, names:[name]}});
  toast(r.code === 200 ? ' Deleted' : ('Error:  ' + r.message));
  if (r.code === 200) loadPath(state.path, true);
}
async function promptOfflineDl() {
  const url = (await askText('Queue offline download', 'HTTP or HTTPS URL', '', 'https://example.com/file')).trim();
  if (!url) return;
  const r = await api('/api/fs/add_offline_download', {method:'POST', json:{path: state.path, urls:[url]}});
  toast(r.code === 200 ? ' Offline download task queued!' : ('Error:  ' + r.message));
}

function openSearch() {
  openModal(`<h3>Instant full-text search (SQLite FTS5)</h3>
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
    return `<div style="padding:9px 12px;border-bottom:1px solid var(--border);cursor:pointer;display:flex;justify-content:space-between" onclick="closeModal();navTo(decodeURIComponent('${safeArg(h.is_dir ? p : h.parent)}'))">
      <span>${h.is_dir?'Folder ':'File '} <b>${esc(h.name)}</b> <small style="color:var(--muted)">(${esc(h.parent)})</small></span>
      <span style="color:var(--muted)">${fmtSize(h.size, h.is_dir)}</span>
    </div>`;
  }).join('');
}

function openCreateShare(path, isDir) {
  openModal(`<h3>Create public share link</h3>
    <p style="font-size:.84rem;color:var(--muted);margin-bottom:12px">Target: <code>${esc(path)}</code></p>
    <div class="grid-2">
      <div class="field"><label>Optional Share Password</label><input class="input" id="shPass" placeholder="Leave blank for open link"></div>
      <div class="field"><label>Expires In (Hours, 0 = Never)</label><input class="input" type="number" id="shExp" value="0"></div>
    </div>
    <div class="field"><label>Max Downloads (0 = Unlimited)</label><input class="input" type="number" id="shMax" value="0"></div>
    <div style="display:flex;gap:8px;justify-content:flex-end;margin-top:14px">
      <button class="btn" onclick="closeModal()">Cancel</button>
      <button class="btn btn-primary" onclick="submitCreateShare(decodeURIComponent('${safeArg(path)}'),${isDir})">Create Share</button>
    </div>`);
}
async function submitCreateShare(path, is_dir) {
  const password = document.getElementById('shPass').value;
  const expH = Number(document.getElementById('shExp').value || 0);
  const max_downloads = Number(document.getElementById('shMax').value || 0);
  const r = await api('/api/share/create', {method:'POST', json:{path, is_dir, password, expires_hours: expH, max_downloads}});
  if (r.code !== 200) { toast('Error:  ' + r.message); return; }
  const url = location.origin + '/s/' + r.data.share_id;
  openModal(`<h3> Share Link Ready</h3>
    <div class="field"><label>Public URL</label><input class="input" readonly value="${esc(url)}" onclick="this.select()"></div>
    <div style="display:flex;gap:8px;justify-content:flex-end">
      <button class="btn btn-primary" onclick="navigator.clipboard?.writeText(decodeURIComponent('${safeArg(url)}'));toast('Copied!')">Copy URL</button>
      <button class="btn" onclick="closeModal()">Done</button>
    </div>`);
}

// PUBLIC SHARE VIEWER
async function loadPublicShare(sub) {
  state.shareSub = sub || '/';
  const info = await api('/api/public/share/info?share_id=' + encodeURIComponent(state.shareId));
  const body = document.getElementById('shareBody');
  if (info.code !== 200) { body.innerHTML = `<div class="empty">Error:  ${esc(info.message)}</div>`; return; }
  if (info.data.has_password && !state.shareToken) {
    body.innerHTML = `<div class="empty"><p style="margin-bottom:12px"> Password-protected share</p>
      <div style="max-width:300px;margin:0 auto;display:flex;gap:8px">
        <input class="input" type="password" id="shAuthPw" placeholder="Enter share password">
        <button class="btn btn-primary" onclick="authPublicShare()">Unlock</button>
      </div></div>`;
    return;
  }
  document.getElementById('shareMetaBadge').textContent = 'Shared by ' + info.data.creator;
  if (!info.data.is_dir) {
    const dl = `/sd/${encodeURIComponent(state.shareId)}?token=${encodeURIComponent(state.shareToken)}`;
    body.innerHTML = `<div class="empty"><h3 style="margin-bottom:10px">File  ${esc(info.data.name)}</h3>
      <a class="btn btn-primary" href="${dl}">⬇ Download Shared File</a></div>`;
    return;
  }
  const ls = await api('/api/public/share/list', {method:'POST', headers:{'X-Share-Token':state.shareToken}, json:{share_id:state.shareId, path:state.shareSub}});
  if (ls.code !== 200) { body.innerHTML = `<div class="empty">Error:  ${esc(ls.message)}</div>`; return; }
  body.innerHTML = `<table><thead><tr><th>Name</th><th>Size</th><th style="text-align:right">Download</th></tr></thead><tbody>` +
    (ls.data.content || []).map(e => {
      const sp = joinPath(state.shareSub, e.name);
      return `<tr onclick="${e.is_dir ? `loadPublicShare(decodeURIComponent('${safeArg(sp)}'))` : ''}">
        <td><div class="fname"><span class="ficon">${iconFor(e)}</span><span>${esc(e.name)}</span></div></td>
        <td>${fmtSize(e.size, e.is_dir)}</td>
        <td style="text-align:right">${!e.is_dir ? `<a class="btn" href="/sd/${encodeURIComponent(state.shareId)}${encodePath(sp)}?token=${encodeURIComponent(state.shareToken)}">⬇</a>` : ''}</td>
      </tr>`;
    }).join('') + `</tbody></table>`;
}
async function authPublicShare() {
  const password = document.getElementById('shAuthPw').value;
  const r = await api('/api/public/share/auth', {method:'POST', json:{share_id: state.shareId, password}});
  if (r.code !== 200) { toast('Error:  ' + r.message); return; }
  state.shareToken = r.data.token;
  sessionStorage.setItem('share_tok', state.shareToken);
  loadPublicShare(state.shareSub);
}

// ADMIN CONSOLE
async function toggleAdminView() {
  if(state.telemetryTimer){clearInterval(state.telemetryTimer);state.telemetryTimer=null;}
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
  if(state.telemetryTimer){clearInterval(state.telemetryTimer);state.telemetryTimer=null;}
  document.querySelectorAll('.tabs .tab').forEach(t => t.classList.remove('active'));
  if (el) el.classList.add('active');
  if (tab === 'storages') loadAdminStorages();
  else if (tab === 'users') loadAdminUsers();
  else if (tab === 'roles') loadAdminRoles();
  else if (tab === 'metas') loadAdminMetas();
  else if (tab === 'shares') loadAdminShares();
  else if (tab === 'tasks') loadAdminTasks();
  else if (tab === 'telemetry') loadAdminTelemetry();
  else if (tab === 'settings') loadAdminSettings();
}
async function loadAdminTelemetry(){
  document.getElementById('adminPanelBody').innerHTML='<h3 style="margin-bottom:12px">Live system telemetry</h3><div id="telemetryGrid" class="grid-2"></div><small style="color:var(--muted)">Refreshes every 2 seconds. Counters are process-lifetime totals.</small>';
  const refresh=async()=>{const r=await api('/api/admin/system/stats');if(r.code!==200)return;const d=r.data;const metrics=[['Version',d.version],['Uptime',`${Math.floor(d.uptime_sec/3600)}h ${Math.floor(d.uptime_sec%3600/60)}m`],['RSS',fmtSize(d.rss_bytes)],['Peak RSS',fmtSize(d.peak_rss_bytes)],['Threads',d.threads],['Connections',d.active_connections],['Requests',d.total_requests],['Network sent',fmtSize(d.bytes_sent)],['Network received',fmtSize(d.bytes_received)],['VFS cache hits / misses',`${d.vfs_cache_hits} / ${d.vfs_cache_misses}`],['Cache entries',d.vfs_cache_entries],['Mounted storages',d.mounted_storages],['DB size',fmtSize(d.db_size_bytes)],['Indexed files',d.fts_indexed_files],['Tasks active / done',`${d.tasks_undone} / ${d.tasks_done}`]];document.getElementById('telemetryGrid')?.replaceChildren(...metrics.map(([k,v])=>{const card=document.createElement('div');card.className='card';card.style.padding='12px';const label=document.createElement('small');label.style.color='var(--muted)';label.textContent=k;const value=document.createElement('div');value.style.fontWeight='650';value.textContent=v;card.append(label,value);return card;}));};
  await refresh();state.telemetryTimer=setInterval(refresh,2000);
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
      <button class="btn" onclick="openStorageModal(${s.id})">Edit</button>
      <button class="btn btn-danger" onclick="delStorage(${s.id})">Delete</button>
    </div></td>
  </tr>`).join('') + `</tbody></table>`;
}

async function openStorageModal(id = 0) {
  let s={mount_path:'',driver:state.drivers[0]?.name||'',cache_expiration:30,enable_sign:false,addition:'{}',order_by:'name',order_direction:'ASC'};
  if(id){const r=await api('/api/admin/storage/get?id='+id);if(r.code!==200){toast(r.message||'Storage not found');return;}s=r.data;}
  const opts = state.drivers.map(d => `<option value="${esc(d.name)}" ${d.name===s.driver?'selected':''}>${esc(d.display_name || d.name)}</option>`).join('');
  openModal(`<h3>${id?'Edit':'Mount'} storage</h3><input type="hidden" id="stId" value="${id}">
    <div class="grid-2"><div class="field"><label>Mount path</label><input class="input" id="stMount" value="${esc(s.mount_path)}" placeholder="/local or /s3"></div><div class="field"><label>Driver</label><select class="input" id="stDrv" onchange="renderDriverFields()">${opts}</select></div></div>
    <div class="grid-2"><div class="field"><label>Cache TTL (minutes)</label><input class="input" type="number" id="stCache" value="${Number(s.cache_expiration)||0}"></div><div class="field"><label>HMAC sign links</label><select class="input" id="stSign"><option value="false" ${!s.enable_sign?'selected':''}>No</option><option value="true" ${s.enable_sign?'selected':''}>Yes</option></select></div></div>
    <hr style="border-color:var(--border);margin:12px 0"><div id="drvFields"></div>
    <div style="display:flex;gap:8px;justify-content:flex-end;margin-top:14px"><button class="btn" onclick="closeModal()">Cancel</button><button class="btn btn-primary" onclick="saveStorage()">${id?'Save changes':'Mount'}</button></div>`);
  renderDriverFields();
  if(id){let addition={};try{addition=JSON.parse(s.addition||'{}')}catch{};document.querySelectorAll('#drvFields [data-df]').forEach(el=>{if(Object.prototype.hasOwnProperty.call(addition,el.dataset.df))el.value=String(addition[el.dataset.df]);});}
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
  const id = Number(document.getElementById('stId').value || 0);
  const r = await api(id ? '/api/admin/storage/update' : '/api/admin/storage/create', {method:'POST', json:{id, mount_path, driver, cache_expiration, enable_sign, addition: JSON.stringify(add)}});
  if (r.code !== 200) { toast('Error:  ' + r.message); return; }
  closeModal();
  toast(' Storage mounted!');
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
  if (r.code !== 200) { toast('Error:  ' + r.message); return; }
  closeModal(); loadAdminUsers();
}

const rolePermissions = ['See hidden items','Bypass folder password','Queue offline downloads','Write','Rename','Move','Copy','Delete','WebDAV read','WebDAV manage','FTP access','FTP manage','Read archives','Extract archives','Limit paths','MCP access','MCP manage'];
async function loadAdminRoles() {
  const r = await api('/api/admin/role/list');
  const items = r.data?.content || [];
  document.getElementById('adminPanelBody').innerHTML = `<div style="display:flex;justify-content:space-between;margin-bottom:14px"><h3>RBAC Roles (${items.length})</h3><button class="btn btn-primary" onclick="openRoleModal()">Create role</button></div>
    <table><thead><tr><th>ID</th><th>Name</th><th>Description</th><th>Scopes</th><th>Actions</th></tr></thead><tbody>` +
    items.map(ro => `<tr><td>${ro.id}</td><td><b>${esc(ro.name)}</b></td><td>${esc(ro.description)}</td><td>${ro.permission_scopes?.length||0}</td><td><button class="btn" onclick="openRoleModal(${ro.id})">Edit</button></td></tr>`).join('') +
    `</tbody></table>`;
}
function roleScopeHtml(scope = {}) {
  const mask = Number(scope.permission) || 0;
  return `<section class="role-scope card" style="padding:12px;margin:10px 0"><div style="display:flex;gap:8px"><input class="input scope-path" aria-label="Scope path" value="${esc(scope.path||'/')}" placeholder="/path"><button class="btn btn-danger" onclick="this.closest('.role-scope').remove()">Remove scope</button></div><div class="grid-2" style="margin-top:10px">${rolePermissions.map((label,bit)=>`<label style="display:flex;gap:8px;align-items:center"><input type="checkbox" data-perm="${bit}" ${(mask & (1<<bit))?'checked':''}>${label}</label>`).join('')}</div></section>`;
}
async function openRoleModal(id = 0) {
  let role = {name:'',description:'',permission_scopes:[{path:'/',permission:0}]};
  if (id) { const r=await api('/api/admin/role/list'); role=(r.data?.content||[]).find(x=>x.id===id)||role; }
  openModal(`<h3>${id?'Edit':'Create'} RBAC role</h3><div class="field"><label>Name</label><input class="input" id="roleName" value="${esc(role.name)}"></div><div class="field"><label>Description</label><input class="input" id="roleDescription" value="${esc(role.description)}"></div><div id="roleScopes">${(role.permission_scopes?.length?role.permission_scopes:[{path:'/',permission:0}]).map(roleScopeHtml).join('')}</div><button class="btn" onclick="addRoleScope()">Add path scope</button><div style="display:flex;justify-content:flex-end;gap:8px;margin-top:14px"><button class="btn" onclick="closeModal()">Cancel</button><button class="btn btn-primary" onclick="saveRole(${id})">Save role</button></div>`);
}
function addRoleScope() { document.getElementById('roleScopes').insertAdjacentHTML('beforeend',roleScopeHtml({path:'/',permission:0})); }
async function saveRole(id) {
  const permission_scopes=[...document.querySelectorAll('.role-scope')].map(row=>{let permission=0;row.querySelectorAll('[data-perm]').forEach(c=>{if(c.checked)permission|=1<<Number(c.dataset.perm)});return {path:row.querySelector('.scope-path').value.trim(),permission};}).filter(s=>s.path);
  const payload={name:document.getElementById('roleName').value.trim(),description:document.getElementById('roleDescription').value.trim(),permission_scopes};
  if(!payload.name){toast('Role name is required');return;}
  const r=await api(id?'/api/admin/role/update':'/api/admin/role/create',{method:'POST',json:id?{...payload,id}:payload});
  if(r.code!==200){toast('Role save failed: '+(r.message||'request failed'));return;}
  closeModal();loadAdminRoles();
}

async function loadAdminMetas() {
  const r = await api('/api/admin/meta/list');
  const items = r.data?.content || [];
  document.getElementById('adminPanelBody').innerHTML = `<div style="display:flex;justify-content:space-between;margin-bottom:14px">
    <h3>Path Metas (${items.length})</h3>
    <button class="btn btn-primary" onclick="openMetaModal()">+ Add Meta Rule</button>
  </div><table><thead><tr><th>Path</th><th>Password</th><th>Hide Globs</th><th style="text-align:right">Actions</th></tr></thead><tbody>` +
  items.map(m => `<tr><td><code>${esc(m.path)}</code></td><td>${m.password?'Protected':'-'}</td><td><code>${esc(m.hide)}</code></td>
    <td style="text-align:right"><button class="btn" onclick="openMetaModal(${m.id})">Edit</button> <button class="btn btn-danger" onclick="api('/api/admin/meta/delete?id=${m.id}',{method:'POST'}).then(loadAdminMetas)">Delete</button></td></tr>`).join('') +
  `</tbody></table>`;
}
async function openMetaModal(id = 0) {
  let m={path:'/local',password:'',p_sub:true,write:false,w_sub:false,hide:'',h_sub:true,readme:'',r_sub:true};
  if(id){const r=await api('/api/admin/meta/list');m=(r.data?.content||[]).find(x=>x.id===id)||m;}
  openModal(`<h3>${id?'Edit':'Add'} path meta rule</h3><input type="hidden" id="mtId" value="${id}">
    <div class="field"><label>VFS path</label><input class="input" id="mtPath" value="${esc(m.path)}"></div>
    <div class="field"><label>Folder password (leave ******** unchanged to preserve)</label><input class="input" id="mtPass" value="${esc(m.password||'')}"></div>
    <div class="field"><label>Hide patterns (newline-separated globs)</label><textarea class="input" id="mtHide">${esc(m.hide||'')}</textarea></div>
    <div class="field"><label>README text</label><textarea class="input" id="mtReadme">${esc(m.readme||'')}</textarea></div>
    <div class="grid-2">${[['mtPSub','Protect subpaths','p_sub'],['mtWrite','Allow writes','write'],['mtWSub','Write on subpaths','w_sub'],['mtHSub','Apply hide rules to subpaths','h_sub'],['mtRSub','Show README on subpaths','r_sub']].map(([id,label,key])=>`<label><input type="checkbox" id="${id}" ${m[key]?'checked':''}> ${label}</label>`).join('')}</div>
    <div style="display:flex;gap:8px;justify-content:flex-end;margin-top:14px"><button class="btn" onclick="closeModal()">Cancel</button><button class="btn btn-primary" onclick="saveMeta()">Save rule</button></div>`);
}
async function saveMeta() {
  const id=Number(document.getElementById('mtId').value);
  const payload={id,path:document.getElementById('mtPath').value.trim(),password:document.getElementById('mtPass').value,hide:document.getElementById('mtHide').value,readme:document.getElementById('mtReadme').value,p_sub:document.getElementById('mtPSub').checked,write:document.getElementById('mtWrite').checked,w_sub:document.getElementById('mtWSub').checked,h_sub:document.getElementById('mtHSub').checked,r_sub:document.getElementById('mtRSub').checked};
  const r=await api(id?'/api/admin/meta/update':'/api/admin/meta/create',{method:'POST',json:payload});
  if(r.code!==200){toast('Save failed: '+(r.message||'request failed'));return;}
  closeModal();loadAdminMetas();
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
      <button class="btn" onclick="loadAdminTasks()">Refresh</button>
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
      <button class="btn btn-primary" onclick="api('/api/admin/index/build',{method:'POST'}).then(()=>{toast(' Search index rebuilt!');loadAdminSettings();})"> Rebuild Full Index Now</button>
    </div>
  </div>`;
}
async function saveSettings() {
  await api('/api/admin/setting/save', {method:'POST', json:[
    {key:'site_title', value: document.getElementById('stTitle').value},
    {key:'sign_all', value: document.getElementById('stSignAll').value},
    {key:'max_devices', value: document.getElementById('stMaxDev').value}
  ]});
  toast(' Settings saved');
  initApp();
}

document.addEventListener('keydown', event => {
  if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === 'k') {
    event.preventDefault(); openSearch();
  } else if (event.key === 'Escape' && document.getElementById('modalBg').classList.contains('open')) {
    closeModal();
  }
});
const explorerDrop = document.getElementById('explorerView');
['dragenter','dragover'].forEach(type => explorerDrop.addEventListener(type, event => {
  event.preventDefault(); if (event.dataTransfer) event.dataTransfer.dropEffect = 'copy'; explorerDrop.classList.add('drop-active');
}));
['dragleave','drop'].forEach(type => explorerDrop.addEventListener(type, event => {
  event.preventDefault(); explorerDrop.classList.remove('drop-active');
}));
explorerDrop.addEventListener('drop', event => {
  if (document.getElementById('writeToolbar').style.display !== 'none' && event.dataTransfer?.files?.length) uploadFiles(event.dataTransfer.files);
});
initApp();
</script>
</body>
</html>
)HTML";

} // namespace alist
