// Unexposed-by-default end-to-end experiment, not a Kova-compatible API.
// Session login remains experimental and requires pre-provisioned identities.
import { authenticateSession, revokeSession, hashSessionToken, sessionFromRequest, browserSessionCookie } from './sessions.js';
import { readGoogleConnection, deleteGoogleConnection } from './credential-vault.js';
import { googleDriveRequest } from './google-drive.js';

const json = (status, data) => new Response(JSON.stringify(data), { status, headers: {
  'content-type': 'application/json; charset=utf-8', 'cache-control': 'no-store', 'x-content-type-options': 'nosniff',
} });
const fail = (status, message) => json(status, { error: message });
const connectionId = /^[A-Za-z0-9_-]{22}$/;

export async function connectedDriveRequest(request, env) {
  if (env?.KOVA_EXPERIMENTAL_V2 !== 'enabled') return fail(404, 'Not found');
  const url = new URL(request.url);
  const drive = url.pathname === '/v2/google-drive/files' || url.pathname === '/v2/google-drive/file' || url.pathname === '/v2/google-drive/export';
  const me = url.pathname === '/v2/me';
  const logout = url.pathname === '/v2/logout';
  const connections = url.pathname === '/v2/connections';
  if (!drive && !me && !logout && !connections) return fail(404, 'Not found');
  if ((logout && request.method !== 'POST') || (!logout && request.method !== 'GET' && !(connections && request.method === 'DELETE'))) return fail(405, 'Method not allowed');
  if (!env.DB || (drive && typeof env.KOVA_CREDENTIAL_KEY !== 'string')) return fail(503, 'Experimental backend is not configured');
  const token = sessionFromRequest(request, env);
  if (!token) return fail(401, 'Unauthorized');
  const connection = drive ? url.searchParams.get('connection') : null;
  if (drive && (url.searchParams.getAll('connection').length !== 1 || !connection || !connectionId.test(connection))) return fail(400, 'Invalid request');
  try {
    const userId = await authenticateSession(env.DB, token);
    if (!userId) return fail(401, 'Unauthorized');
    if (me) return json(200, { id: userId });
    if (connections && request.method === 'DELETE') {
      const ids = url.searchParams.getAll('id');
      if (ids.length !== 1 || !connectionId.test(ids[0])) return fail(400, 'Invalid request');
      const deleted = await deleteGoogleConnection(env.DB, userId, ids[0], await hashSessionToken(token));
      // Deletion removes the stored refresh token locally; it does not revoke
      // Google's grant or terminate downloads already in progress.
      return deleted ? json(200, { ok: true, providerRevoked: false }) : fail(404, 'Not found');
    }
    if (connections) {
      const cursors = url.searchParams.getAll('cursor');
      if (cursors.length > 1 || (cursors.length === 1 && !connectionId.test(cursors[0]))) return fail(400, 'Invalid request');
      const cursor = cursors[0] || '';
      const result = await env.DB.prepare(`SELECT c.id,c.provider,c.created_at FROM provider_connections AS c
        JOIN users AS u ON u.id = c.owner_id AND u.disabled = 0
        WHERE c.owner_id = ? AND c.id > ? ORDER BY c.id LIMIT 51`).bind(userId, cursor).all();
      if (!Array.isArray(result.results)) throw Error('Invalid connection listing');
      const hasMore = result.results.length > 50;
      const items = result.results.slice(0, 50).map(({ id, provider, created_at }) => ({ id, provider, createdAt: created_at }));
      return json(200, { connections: items, nextCursor: hasMore ? items.at(-1).id : null });
    }
    if (logout) {
      // Do not accept a user ID or another session token in the request body.
      const revoked = await revokeSession(env.DB, token);
      if (!revoked) return fail(401, 'Unauthorized');
      const response = json(200, { ok: true });
      if (env.KOVA_EXPERIMENTAL_BROWSER_AUTH === 'enabled') response.headers.append('set-cookie', browserSessionCookie('', 0));
      return response;
    }
    // The vault verifies enabled ownership in D1; do not allow a client to
    // choose userId or provide provider credentials in the request.
    const credentials = await readGoogleConnection(env.DB, env.KOVA_CREDENTIAL_KEY, userId, connection);
    if (!credentials) return fail(404, 'Not found');
    const upstreamUrl = new URL(url);
    upstreamUrl.pathname = url.pathname.replace('/v2/', '/v1/');
    upstreamUrl.searchParams.delete('connection');
    return await googleDriveRequest(upstreamUrl, {
      GOOGLE_CLIENT_ID: credentials.clientId,
      GOOGLE_CLIENT_SECRET: credentials.clientSecret,
      GOOGLE_REFRESH_TOKEN: credentials.refreshToken,
    }, request.headers);
  } catch {
    return fail(503, 'Experimental backend unavailable');
  }
}
