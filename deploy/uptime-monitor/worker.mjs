// Independent, opt-in monitor. No arbitrary targets or credentials accepted
// from HTTP callers; only the scheduler can issue outbound requests.
const LIMIT = 288;
const INTERVAL = 5 * 60 * 1000;
const headers = { 'Cache-Control': 'no-store', 'X-Content-Type-Options': 'nosniff', 'Referrer-Policy': 'no-referrer' };

export function targetUrl(raw) {
  if (typeof raw !== 'string' || !/^https:\/\/[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?\.onrender\.com$/.test(raw)) return null;
  return `${raw}/ping`;
}

async function pong(response) {
  if (response.status !== 200 || !response.body) { try { await response.body?.cancel(); } catch {} return false; }
  const reader = response.body.getReader();
  let text = '';
  try {
    const decoder = new TextDecoder();
    for (;;) {
      const { value, done } = await reader.read();
      if (done) break;
      if (value.byteLength > 16 || text.length + value.byteLength > 16) return false;
      text += decoder.decode(value, { stream: true });
    }
    return text + decoder.decode() === 'pong';
  } finally { try { await reader.cancel(); } catch { /* closed */ } }
}

export async function sample(env, time, fetcher = fetch) {
  const url = targetUrl(env.RENDER_ORIGIN);
  if (!url || !env.MONITOR_KV) throw new Error('Monitor configuration is missing or invalid');
  const start = Date.now();
  const abort = new AbortController();
  const timer = setTimeout(() => abort.abort(), 75000);
  let up = false;
  try {
    const response = await fetcher(url, { method: 'GET', redirect: 'manual', signal: abort.signal, headers: { 'User-Agent': 'Kova-Uptime-Monitor/1.0' } });
    up = await pong(response);
  } catch { up = false; }
  finally { clearTimeout(timer); }
  const ms = Math.min(75000, Math.max(0, Date.now() - start));
  let previous = [];
  try { const value = await env.MONITOR_KV.get('samples', 'json'); if (Array.isArray(value)) previous = value; } catch { throw new Error('Monitor history unavailable'); }
  // Ignore delayed or duplicate events rather than replacing newer samples. KV
  // is eventually consistent, so concurrent invocations can still lose writes.
  if (previous.some(x => x && Number.isSafeInteger(x.t) && x.t >= time)) return;
  const valid = previous.filter(x => x && Number.isSafeInteger(x.t) && typeof x.up === 'boolean' && Number.isFinite(x.ms) && x.ms >= 0 && x.ms <= 75000 && x.t < time && x.t >= time - 24 * 60 * 60 * 1000);
  valid.push({ t: time, up, ms });
  await env.MONITOR_KV.put('samples', JSON.stringify(valid.slice(-LIMIT)));
}

export function summarize(value, now = Date.now()) {
  const rows = Array.isArray(value) ? value.filter(x => x && Number.isSafeInteger(x.t) && x.t <= now && x.t >= now - 24 * 60 * 60 * 1000 && typeof x.up === 'boolean' && Number.isFinite(x.ms) && x.ms >= 0 && x.ms <= 75000).slice(-LIMIT) : [];
  rows.sort((a, b) => a.t - b.t);
  const last = rows.at(-1);
  return { state: !last ? 'unknown' : now - last.t > 3 * INTERVAL ? 'stale' : last.up ? 'up' : 'down', checkedAt: last?.t ?? null, responseMs: last?.ms ?? null, successful: rows.filter(x => x.up).length, samples: rows.length, windowHours: 24 };
}

const page = `<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Kova status</title><style>body{font:16px system-ui,sans-serif;max-width:44rem;margin:10vh auto;padding:0 1.5rem;color:#182338;background:#f3f6fa}main{background:#fff;padding:2rem;border-radius:14px}h1{font-size:1.6rem}#state{font-size:1.25rem;font-weight:700}p{line-height:1.6}small{color:#46556a}</style></head><body><main><h1>Kova service status</h1><p id="state" role="status">Loading…</p><p id="detail"></p><small>Samples reflect /ping only. Missing checks, cold starts, storage, sign-in and providers are not included. This page is public.</small></main><script src="/status.js" defer></script></body></html>`;
const script = `fetch('/api/status',{cache:'no-store'}).then(r=>{if(!r.ok)throw Error();return r.json()}).then(x=>{document.getElementById('state').textContent=({up:'Responding',down:'Not responding',stale:'Monitor data is stale',unknown:'No checks yet'})[x.state]||'Unknown';document.getElementById('detail').textContent=x.checkedAt?'Last check: '+new Date(x.checkedAt).toLocaleString()+' · '+x.successful+'/'+x.samples+' successful samples in the last 24 hours.':'Waiting for the first scheduled check.'}).catch(()=>{document.getElementById('state').textContent='Monitor unavailable';});`;

export default {
  async scheduled(event, env) { await sample(env, event.scheduledTime); },
  async fetch(request, env) {
    const path = new URL(request.url).pathname;
    if (request.method !== 'GET') return new Response('Method not allowed', { status: 405, headers });
    if (path === '/status.js') return new Response(script, { headers: { ...headers, 'Content-Type': 'text/javascript; charset=utf-8' } });
    if (path === '/') return new Response(page, { headers: { ...headers, 'Content-Type': 'text/html; charset=utf-8', 'Content-Security-Policy': "default-src 'none'; script-src 'self'; style-src 'unsafe-inline'; connect-src 'self'; base-uri 'none'; frame-ancestors 'none'" } });
    if (path !== '/api/status') return new Response('Not found', { status: 404, headers });
    try {
      const value = await env.MONITOR_KV.get('samples', 'json');
      return Response.json(summarize(value), { headers });
    } catch { return Response.json({ error: 'Monitor unavailable' }, { status: 503, headers }); }
  }
};
