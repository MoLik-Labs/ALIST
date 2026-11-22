// Experimental single-account, read-only Google Drive connector.
// Credentials stay in Worker secrets; this is not a multi-user authorization system.
const TOKEN_URL = 'https://oauth2.googleapis.com/token';
const DRIVE_URL = 'https://www.googleapis.com/drive/v3/files';
const MAX_JSON_BYTES = 1024 * 1024;
// Fixed export types prevent arbitrary upstream content negotiation.
const EXPORT_FORMATS = Object.freeze({
  pdf: 'application/pdf',
  docx: 'application/vnd.openxmlformats-officedocument.wordprocessingml.document',
  xlsx: 'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet',
  pptx: 'application/vnd.openxmlformats-officedocument.presentationml.presentation',
  txt: 'text/plain',
  csv: 'text/csv',
});
const error = (status, message) => json(status, { error: message });
const json = (status, data) => new Response(JSON.stringify(data), { status, headers: {
  'content-type': 'application/json; charset=utf-8', 'cache-control': 'no-store', 'x-content-type-options': 'nosniff',
} });

async function readJson(response) {
  if (!response.body) throw new Error('Missing upstream body');
  const reader = response.body.getReader();
  const parts = [];
  let size = 0;
  try {
    while (true) {
      const { done, value } = await reader.read();
      if (done) break;
      size += value.byteLength;
      if (size > MAX_JSON_BYTES) {
        await reader.cancel().catch(() => {});
        throw new Error('Upstream body too large');
      }
      parts.push(value);
    }
  } finally { reader.releaseLock(); }
  const bytes = new Uint8Array(size);
  let offset = 0;
  for (const part of parts) { bytes.set(part, offset); offset += part.byteLength; }
  return JSON.parse(new TextDecoder().decode(bytes));
}

const validSecret = value => typeof value === 'string' && value.length > 0 && value.length <= 4096 && !/[\x00-\x1f\x7f]/.test(value);
const validId = value => typeof value === 'string' && /^[A-Za-z0-9_-]{1,200}$/.test(value);
const fetchOptions = { redirect: 'manual' };

// Single HTTP byte range only. Reject malformed/multipart values before making
// upstream calls. Integer strings are bounded before conversion to BigInt.
function parseRange(value) {
  if (value === null) return null;
  const m = /^bytes=(\d{1,19})?-(\d{1,19})?$/.exec(value);
  if (!m || (!m[1] && !m[2])) return false;
  const max = 9223372036854775807n;
  const start = m[1] === undefined ? null : BigInt(m[1]);
  const end = m[2] === undefined ? null : BigInt(m[2]);
  if ((start !== null && start > max) || (end !== null && end > max) || (start === null && end === 0n) || (start !== null && end !== null && end < start)) return false;
  return { start, end };
}
function validPartial(range, value, length) {
  const match = /^bytes (\d{1,19})-(\d{1,19})\/(\d{1,19})$/.exec(value || '');
  if (!match) return false;
  const start = BigInt(match[1]), end = BigInt(match[2]), total = BigInt(match[3]);
  if (total === 0n || start > end || end >= total) return false;
  const expectedStart = range.start === null ? (range.end >= total ? 0n : total - range.end) : range.start;
  const expectedEnd = range.start === null || range.end === null || range.end >= total ? total - 1n : range.end;
  if (start !== expectedStart || end !== expectedEnd) return false;
  if (length !== null && (!/^\d{1,19}$/.test(length) || BigInt(length) !== end - start + 1n)) return false;
  return true;
}

export async function googleDriveRequest(url, env, requestHeaders = new Headers()) {
  const list = url.pathname === '/v1/google-drive/files';
  const download = url.pathname === '/v1/google-drive/file';
  const exporting = url.pathname === '/v1/google-drive/export';
  if (!list && !download && !exporting) return error(404, 'Not found');
  const formats = url.searchParams.getAll('format');
  if (exporting && (formats.length !== 1 || !Object.hasOwn(EXPORT_FORMATS, formats[0]) || requestHeaders.has('range'))) return error(400, 'Invalid export request');
  const idName = list ? 'parent' : 'id';
  const id = url.searchParams.get(idName) || (list ? 'root' : '');
  const cursor = url.searchParams.get('cursor') || '';
  const name = list ? url.searchParams.get('name') : null;
  const range = download ? parseRange(requestHeaders.get('range')) : null;
  if (range === false) return error(400, 'Invalid Range header');
  // Reject ambiguous inputs and escape user text before embedding it in a Drive query.
  if (url.searchParams.getAll(idName).length > 1 || url.searchParams.getAll('cursor').length > 1 ||
      (list && url.searchParams.getAll('name').length > 1) || !validId(id) || cursor.length > 2048 || /[\x00-\x1f\x7f]/.test(cursor) ||
      (name !== null && (!name || name.length > 200 || /[\x00-\x1f\x7f]/.test(name)))) return error(400, 'Invalid request');
  const { GOOGLE_CLIENT_ID, GOOGLE_CLIENT_SECRET, GOOGLE_REFRESH_TOKEN } = env;
  if (![GOOGLE_CLIENT_ID, GOOGLE_CLIENT_SECRET, GOOGLE_REFRESH_TOKEN].every(validSecret)) return error(503, 'Google Drive is not configured');
  try {
    const tokenResponse = await fetch(TOKEN_URL, {
      ...fetchOptions, method: 'POST', signal: AbortSignal.timeout(15000),
      headers: { 'content-type': 'application/x-www-form-urlencoded' },
      body: new URLSearchParams({ client_id: GOOGLE_CLIENT_ID, client_secret: GOOGLE_CLIENT_SECRET, refresh_token: GOOGLE_REFRESH_TOKEN, grant_type: 'refresh_token' }),
    });
    if (!tokenResponse.ok) return error(502, 'Google authorization failed');
    const token = (await readJson(tokenResponse)).access_token;
    if (!validSecret(token) || /\s/.test(token)) return error(502, 'Google authorization failed');
    const target = new URL(list ? DRIVE_URL : `${DRIVE_URL}/${encodeURIComponent(id)}${exporting ? '/export' : ''}`);
    if (list) {
      let query = `'${id}' in parents and trashed = false`;
      if (name !== null) query += ` and name contains '${name.replace(/\\/g, '\\\\').replace(/'/g, "\\'")}'`;
      target.searchParams.set('q', query);
      target.searchParams.set('fields', 'nextPageToken,files(id,name,mimeType,size,modifiedTime)');
      target.searchParams.set('pageSize', '100');
      target.searchParams.set('supportsAllDrives', 'true');
      target.searchParams.set('includeItemsFromAllDrives', 'true');
      if (cursor) target.searchParams.set('pageToken', cursor);
    } else if (exporting) {
      target.searchParams.set('mimeType', EXPORT_FORMATS[formats[0]]);
    } else {
      target.searchParams.set('alt', 'media');
      target.searchParams.set('supportsAllDrives', 'true');
    }
    const headers = { authorization: `Bearer ${token}` };
    if (range) headers.range = requestHeaders.get('range');
    // A fixed AbortSignal.timeout on a streaming response also aborts its
    // body, truncating downloads after 30 seconds. Limit time to headers for
    // downloads instead; retain the full deadline for bounded JSON listings.
    let upstream;
    if (download || exporting) {
      const controller = new AbortController();
      const timer = setTimeout(() => controller.abort(), 30000);
      try {
        upstream = await fetch(target.toString(), { ...fetchOptions, signal: controller.signal, headers });
      } finally { clearTimeout(timer); }
    } else {
      upstream = await fetch(target.toString(), { ...fetchOptions, signal: AbortSignal.timeout(30000), headers });
    }
    if (upstream.status === 404) return error(404, 'Not found');
    if (upstream.status === 429 || upstream.status === 503) return error(503, 'Google Drive temporarily unavailable');
    if (download && range && upstream.status === 416) {
      const value = upstream.headers.get('content-range');
      if (!/^bytes \*\/\d{1,19}$/.test(value || '')) return error(502, 'Google Drive request failed');
      return new Response(null, { status: 416, headers: { 'content-range': value, 'accept-ranges': 'bytes', 'cache-control': 'private, no-store', 'x-content-type-options': 'nosniff' } });
    }
    // A 206 response is valid only for a requested range, with consistent headers.
    const partial = download && !!range && upstream.status === 206;
    if (partial && !validPartial(range, upstream.headers.get('content-range'), upstream.headers.get('content-length'))) return error(502, 'Google Drive request failed');
    if (upstream.status !== 200 && !partial) return error(502, 'Google Drive request failed');
    if (list) {
      const data = await readJson(upstream);
      if (!Array.isArray(data.files)) throw new Error('Invalid response');
      // Return only the explicitly requested metadata, never upstream errors or credentials.
      const files = data.files.map(({ id, name, mimeType, size, modifiedTime }) => ({ id, name, mimeType, size, modifiedTime }));
      return json(200, { files, nextPageToken: typeof data.nextPageToken === 'string' ? data.nextPageToken : null });
    }
    // Do not forward arbitrary upstream headers or redirect locations.
    const safeHeaders = { 'content-type': 'application/octet-stream', 'content-disposition': 'attachment',
      'cache-control': 'private, no-store', 'x-content-type-options': 'nosniff' };
    if (download) safeHeaders['accept-ranges'] = 'bytes';
    const contentLength = upstream.headers.get('content-length');
    if (contentLength && /^\d{1,19}$/.test(contentLength)) safeHeaders['content-length'] = contentLength;
    if (partial) safeHeaders['content-range'] = upstream.headers.get('content-range');
    return new Response(upstream.body, { status: partial ? 206 : 200, headers: safeHeaders });
  } catch {
    return error(502, 'Google Drive request failed');
  }
}
