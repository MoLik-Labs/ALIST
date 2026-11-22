// Isolated session primitives. The issuer MUST first establish the user's
// identity with a secure, rate-limited authentication flow. There is no HTTP
// endpoint or password verification here, and these are not production-ready.
const ID = /^[A-Za-z0-9_-]{1,128}$/;
const TOKEN = /^[A-Za-z0-9_-]{43}$/;
const MAX_SESSION_SECONDS = 24 * 60 * 60;
const validTime = n => Number.isSafeInteger(n) && n >= 0;
const validId = id => typeof id === 'string' && ID.test(id);
const nowSeconds = () => Math.floor(Date.now() / 1000);
const hex = bytes => [...new Uint8Array(bytes)].map(b => b.toString(16).padStart(2, '0')).join('');
const hashToken = async token => hex(await crypto.subtle.digest('SHA-256', new TextEncoder().encode(token)));
function makeToken() {
  return btoa(String.fromCharCode(...crypto.getRandomValues(new Uint8Array(32))))
    .replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
}

export async function issueSession(db, verifiedUserId, expiresAt, now = nowSeconds(), requiredIdentity = null) {
  if (!validId(verifiedUserId) || !validTime(now) || !validTime(expiresAt) || expiresAt <= now || expiresAt - now > MAX_SESSION_SECONDS) return null;
  if (requiredIdentity !== null && (requiredIdentity?.provider !== 'google' || typeof requiredIdentity.subject !== 'string' || !/^[\x21-\x7e]{1,255}$/.test(requiredIdentity.subject))) return null;
  const token = makeToken();
  const digest = await hashToken(token);
  const guard = requiredIdentity === null ? '' : ` AND EXISTS (
    SELECT 1 FROM external_identities WHERE provider = ? AND subject = ? AND user_id = users.id)`;
  const values = [digest, now, expiresAt, verifiedUserId];
  if (requiredIdentity !== null) values.push(requiredIdentity.provider, requiredIdentity.subject);
  const result = await db.prepare(`INSERT INTO sessions (token_hash, user_id, created_at, expires_at)
    SELECT ?, id, ?, ? FROM users WHERE id = ? AND disabled = 0${guard}`).bind(...values).run();
  return result.meta?.changes === 1 ? token : null;
}

export async function authenticateSession(db, token, now = nowSeconds()) {
  if (typeof token !== 'string' || !TOKEN.test(token) || !validTime(now)) return null;
  const digest = await hashToken(token);
  const row = await db.prepare(`SELECT u.id AS user_id FROM sessions AS s
    JOIN users AS u ON u.id = s.user_id AND u.disabled = 0
    WHERE s.token_hash = ? AND s.revoked_at IS NULL AND s.expires_at > ? LIMIT 1`)
    .bind(digest, now).first();
  return row?.user_id || null;
}

// The caller provides a validated bearer token; no user ID is accepted from a
// client. The session hash is never returned or logged by these primitives.
export async function revokeSession(db, token, now = nowSeconds()) {
  if (typeof token !== 'string' || !TOKEN.test(token) || !validTime(now)) return false;
  const digest = await hashToken(token);
  const result = await db.prepare('UPDATE sessions SET revoked_at = ? WHERE token_hash = ? AND revoked_at IS NULL')
    .bind(now, digest).run();
  return result.meta?.changes === 1;
}

// Only call after authorization by a separate role/identity system. This
// operation is deliberately separate from the externally callable logout flow.
export async function revokeAllSessionsForUser(db, verifiedUserId, now = nowSeconds()) {
  if (!validId(verifiedUserId) || !validTime(now)) return false;
  await db.prepare('UPDATE sessions SET revoked_at = ? WHERE user_id = ? AND revoked_at IS NULL')
    .bind(now, verifiedUserId).run();
  return true;
}
