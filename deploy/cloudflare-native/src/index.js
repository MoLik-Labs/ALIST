// Experimental standalone Cloudflare-native R2 reader. This is NOT the Kova
// API or a production-ready server. Do not attach production data.
const json = (status, payload) => new Response(JSON.stringify(payload), {
  status,
  headers: { 'content-type': 'application/json; charset=utf-8', 'cache-control': 'no-store', 'x-content-type-options': 'nosniff' },
});

async function authorized(request, token) {
  if (typeof token !== 'string' || token.length < 32 || token.length > 1024) return false;
  const match = /^Bearer ([^\s]+)$/.exec(request.headers.get('authorization') || '');
  if (!match || match[1].length > 1024) return false;
  const encoder = new TextEncoder();
  const [left, right] = await Promise.all([
    crypto.subtle.digest('SHA-256', encoder.encode(token)),
    crypto.subtle.digest('SHA-256', encoder.encode(match[1])),
  ]);
  const a = new Uint8Array(left), b = new Uint8Array(right);
  let diff = 0;
  for (let i = 0; i < a.length; i++) diff |= a[i] ^ b[i];
  return diff === 0;
}

function safeText(value, max) {
  return value.length <= max && !/[\x00-\x1f\x7f]/.test(value);
}

export async function handleRequest(request, env) {
  const url = new URL(request.url);
  if (request.method === 'GET' && url.pathname === '/ping') return json(200, { ok: true, experimental: true });
  if (!env?.R2 || typeof env?.KOVA_NATIVE_TOKEN !== 'string' || env.KOVA_NATIVE_TOKEN.length < 32 || env.KOVA_NATIVE_TOKEN.length > 1024) {
    return json(503, { error: 'Experimental backend is not configured' });
  }
  if (!await authorized(request, env.KOVA_NATIVE_TOKEN)) return json(401, { error: 'Unauthorized' });
  if (request.method !== 'GET') return json(405, { error: 'Method not allowed' });
  try {
    if (url.pathname === '/v1/objects') {
      const prefix = url.searchParams.get('prefix') || '';
      const cursor = url.searchParams.get('cursor') || '';
      if (!safeText(prefix, 1024) || !safeText(cursor, 4096)) return json(400, { error: 'Invalid request' });
      const options = { prefix, limit: 100 };
      if (cursor) options.cursor = cursor;
      const result = await env.R2.list(options);
      return json(200, { objects: result.objects.map(object => ({ key: object.key, size: object.size, uploaded: object.uploaded.toISOString() })), cursor: result.truncated ? result.cursor : null });
    }
    if (url.pathname === '/v1/object') {
      const key = url.searchParams.get('key');
      if (!key || !safeText(key, 1024)) return json(400, { error: 'Invalid request' });
      const object = await env.R2.get(key);
      if (!object) return json(404, { error: 'Not found' });
      return new Response(object.body, { status: 200, headers: {
        'content-type': 'application/octet-stream',
        'content-disposition': 'attachment',
        'content-length': String(object.size),
        'cache-control': 'private, no-store',
        'x-content-type-options': 'nosniff',
      } });
    }
    return json(404, { error: 'Not found' });
  } catch {
    return json(502, { error: 'Storage request failed' });
  }
}

export default { fetch: handleRequest };
