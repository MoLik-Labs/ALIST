// Experimental single-account, read-only Google Drive connector.
// Credentials stay in Worker secrets; this is not a multi-user authorization system.
const TOKEN_URL = 'https://oauth2.googleapis.com/token';
const DRIVE_URL = 'https://www.googleapis.com/drive/v3/files';
const MAX_JSON_BYTES = 1024 * 1024;
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

export async function googleDriveRequest(url, env) {
  const list = url.pathname === '/v1/google-drive/files';
  const download = url.pathname === '/v1/google-drive/file';
  if (!list && !download) return error(404, 'Not found');
  const id = url.searchParams.get(list ? 'parent' : 'id') || (list ? 'root' : '');
  const cursor = url.searchParams.get('cursor') || '';
  // Restrict IDs before interpolating the Drive query and encoding path segments.
  if (!validId(id) || cursor.length > 2048 || /[\x00-\x1f\x7f]/.test(cursor)) return error(400, 'Invalid request');
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
    const target = new URL(list ? DRIVE_URL : `${DRIVE_URL}/${encodeURIComponent(id)}`);
    if (list) {
      target.searchParams.set('q', `'${id}' in parents and trashed = false`);
      target.searchParams.set('fields', 'nextPageToken,files(id,name,mimeType,size,modifiedTime)');
      target.searchParams.set('pageSize', '100');
      target.searchParams.set('supportsAllDrives', 'true');
      target.searchParams.set('includeItemsFromAllDrives', 'true');
      if (cursor) target.searchParams.set('pageToken', cursor);
    } else {
      target.searchParams.set('alt', 'media');
      target.searchParams.set('supportsAllDrives', 'true');
    }
    const upstream = await fetch(target.toString(), {
      ...fetchOptions, signal: AbortSignal.timeout(30000), headers: { authorization: `Bearer ${token}` },
    });
    if (upstream.status === 404) return error(404, 'Not found');
    if (upstream.status === 429 || upstream.status === 503) return error(503, 'Google Drive temporarily unavailable');
    // No range request was sent; an unexpected partial or empty success must
    // not be presented to the client as a complete 200 download/listing.
    if (upstream.status !== 200) return error(502, 'Google Drive request failed');
    if (list) {
      const data = await readJson(upstream);
      if (!Array.isArray(data.files)) throw new Error('Invalid response');
      // Return only the explicitly requested metadata, never upstream errors or credentials.
      const files = data.files.map(({ id, name, mimeType, size, modifiedTime }) => ({ id, name, mimeType, size, modifiedTime }));
      return json(200, { files, nextPageToken: typeof data.nextPageToken === 'string' ? data.nextPageToken : null });
    }
    // Do not forward arbitrary upstream headers or redirect locations.
    return new Response(upstream.body, { status: 200, headers: {
      'content-type': 'application/octet-stream', 'content-disposition': 'attachment',
      'cache-control': 'private, no-store', 'x-content-type-options': 'nosniff',
    } });
  } catch {
    return error(502, 'Google Drive request failed');
  }
}
