const HOP_BY_HOP_HEADERS = [
  'connection',
  'keep-alive',
  'proxy-authenticate',
  'proxy-authorization',
  'te',
  'trailer',
  'transfer-encoding',
  'upgrade',
];

function stripHopByHop(headers) {
  const connectionTokens = (headers.get('connection') || '')
    .split(',')
    .map(value => value.trim().toLowerCase())
    .filter(Boolean);
  for (const name of [...HOP_BY_HOP_HEADERS, ...connectionTokens]) headers.delete(name);
}

function configuredOrigin(value) {
  if (!value) return null;
  try {
    const origin = new URL(value);
    if (origin.protocol !== 'https:' || origin.username || origin.password ||
        origin.pathname !== '/' || origin.search || origin.hash) return null;
    return origin;
  } catch {
    return null;
  }
}

function response(status, message) {
  return new Response(message, {
    status,
    headers: {
      'cache-control': 'no-store',
      'content-type': 'text/plain; charset=utf-8',
      'x-content-type-options': 'nosniff',
    },
  });
}

function rewriteOriginRedirect(headers, origin, incoming) {
  const location = headers.get('location');
  if (!location || !(/^[a-z][a-z0-9+.-]*:/i.test(location) || location.startsWith('//'))) return;
  try {
    const destination = new URL(location, origin);
    if (destination.host !== origin.host || !['http:', 'https:'].includes(destination.protocol)) return;
    destination.protocol = incoming.protocol;
    destination.host = incoming.host;
    headers.set('location', destination.toString());
  } catch {
    // Preserve malformed/opaque Location values rather than rewriting them.
  }
}

export async function handleRequest(request, env, fetchImpl = globalThis.fetch) {
  const origin = configuredOrigin(env?.KOVA_ORIGIN);
  if (!origin) return response(503, 'Kova edge proxy is not configured with a valid HTTPS KOVA_ORIGIN.');

  const incoming = new URL(request.url);
  const upstreamUrl = new URL(`${incoming.pathname}${incoming.search}`, origin);
  const headers = new Headers(request.headers);
  stripHopByHop(headers);
  headers.delete('host');
  headers.delete('cf-connecting-ip');

  // The origin must set ALIST_TRUST_PROXY=true only when this Worker is its
  // trusted entry point. Overwrite client-supplied forwarding headers.
  headers.set('x-forwarded-host', incoming.host);
  headers.set('x-forwarded-proto', incoming.protocol.slice(0, -1));
  const clientIp = request.headers.get('cf-connecting-ip');
  if (clientIp) headers.set('x-forwarded-for', clientIp);
  else headers.delete('x-forwarded-for');

  const init = {
    method: request.method,
    headers,
    redirect: 'manual',
    signal: request.signal,
    cf: { cacheTtl: 0, cacheEverything: false },
  };
  if (request.method !== 'GET' && request.method !== 'HEAD') init.body = request.body;

  try {
    const upstream = await fetchImpl(upstreamUrl, init);
    const responseHeaders = new Headers(upstream.headers);
    stripHopByHop(responseHeaders);
    // Prevent shared-cache rules from persisting authenticated workspace data,
    // signed downloads, or public-share quota responses at the edge.
    responseHeaders.set('cache-control', 'private, no-store');
    rewriteOriginRedirect(responseHeaders, origin, incoming);
    return new Response(upstream.body, {
      status: upstream.status,
      statusText: upstream.statusText,
      headers: responseHeaders,
    });
  } catch {
    return response(502, 'Kova origin is unavailable. Check the configured KOVA_ORIGIN and origin health.');
  }
}

export default {
  fetch(request, env) {
    return handleRequest(request, env);
  },
};
