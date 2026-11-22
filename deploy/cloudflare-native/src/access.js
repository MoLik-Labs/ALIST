// Standalone, unexposed experimental D1 access primitives. These do not grant
// access to the prototype's bearer-token routes and are not a login system.
const ACTIONS = new Set(['read', 'write', 'delete', 'share']);
const ID = /^[A-Za-z0-9_-]{1,128}$/;
const TOKEN = /^[A-Za-z0-9_-]{43}$/;
const MAX_SHARE_SECONDS = 30 * 24 * 60 * 60;
const nowSeconds = () => Math.floor(Date.now() / 1000);
const validId = id => typeof id === 'string' && ID.test(id);
const validTime = time => Number.isSafeInteger(time) && time >= 0;
const hex = bytes => [...new Uint8Array(bytes)].map(v => v.toString(16).padStart(2, '0')).join('');
async function tokenHash(token) { return hex(await crypto.subtle.digest('SHA-256', new TextEncoder().encode(token))); }
function tokenString() {
  const bytes = crypto.getRandomValues(new Uint8Array(32));
  return btoa(String.fromCharCode(...bytes)).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
}

// Owners have all actions; other enabled users require an explicit role grant.
// Admin is intentionally not a magic bypass. A caller must have authenticated
// userId with a separate, trusted session system before invoking this function.
export async function canAccess(db, userId, resourceId, action) {
  if (!validId(userId) || !validId(resourceId) || !ACTIONS.has(action)) return false;
  const row = await db.prepare(`SELECT 1 AS allowed FROM resources AS r
    JOIN users AS u ON u.id = ? AND u.disabled = 0
    WHERE r.id = ? AND r.deleted_at IS NULL AND (
      r.owner_id = u.id OR EXISTS (
        SELECT 1 FROM user_roles AS ur JOIN resource_role_grants AS g ON g.role_id = ur.role_id
        WHERE ur.user_id = u.id AND g.resource_id = r.id AND g.action = ?
      )
    ) LIMIT 1`).bind(userId, resourceId, action).first();
  return !!row;
}

// Anonymous visibility is read-only and independent of role membership.
export async function canReadPublic(db, resourceId) {
  if (!validId(resourceId)) return false;
  return !!await db.prepare("SELECT 1 AS allowed FROM resources AS r JOIN users AS u ON u.id = r.owner_id AND u.disabled = 0 WHERE r.id = ? AND r.visibility = 'public' AND r.deleted_at IS NULL LIMIT 1").bind(resourceId).first();
}

// An explicit share grant is required; tokens are never persisted in plaintext.
export async function createReadShare(db, userId, resourceId, expiresAt, now = nowSeconds()) {
  if (!validId(userId) || !validId(resourceId) || !validTime(now) || !validTime(expiresAt) || expiresAt <= now || expiresAt - now > MAX_SHARE_SECONDS) return null;
  const token = tokenString();
  const digest = await tokenHash(token);
  // Authorization is part of INSERT, avoiding a grant-revocation race between
  // a separate access check and insertion. D1 executes this as one statement.
  const result = await db.prepare(`INSERT INTO bearer_shares (token_hash, resource_id, created_by, expires_at, created_at)
    SELECT ?, r.id, u.id, ?, ? FROM resources AS r JOIN users AS u ON u.id = ? AND u.disabled = 0
    WHERE r.id = ? AND r.deleted_at IS NULL AND (r.owner_id = u.id OR EXISTS (
      SELECT 1 FROM user_roles AS ur JOIN resource_role_grants AS g ON g.role_id = ur.role_id
      WHERE ur.user_id = u.id AND g.resource_id = r.id AND g.action = 'share'
    ))`).bind(digest, expiresAt, now, userId, resourceId).run();
  return result.meta?.changes === 1 ? token : null;
}

export async function resolveReadShare(db, token, now = nowSeconds()) {
  if (typeof token !== 'string' || !TOKEN.test(token) || !validTime(now)) return null;
  const digest = await tokenHash(token);
  const row = await db.prepare(`SELECT s.resource_id FROM bearer_shares AS s
    JOIN resources AS r ON r.id = s.resource_id AND r.deleted_at IS NULL
    JOIN users AS u ON u.id = s.created_by AND u.disabled = 0
    WHERE s.token_hash = ? AND s.revoked_at IS NULL AND s.expires_at > ? LIMIT 1`).bind(digest, now).first();
  return row?.resource_id || null;
}

// Revocation takes a share hash/ID internally, not a bearer token from logs.
// Authorization is checked atomically with the update.
export async function revokeReadShare(db, userId, resourceId, digest, now = nowSeconds()) {
  if (!validId(userId) || !validId(resourceId) || typeof digest !== 'string' || !/^[a-f0-9]{64}$/.test(digest) || !validTime(now)) return false;
  const result = await db.prepare(`UPDATE bearer_shares SET revoked_at = ?
    WHERE token_hash = ? AND resource_id = ? AND revoked_at IS NULL AND EXISTS (
      SELECT 1 FROM resources AS r JOIN users AS u ON u.id = ? AND u.disabled = 0
      WHERE r.id = bearer_shares.resource_id AND r.deleted_at IS NULL AND (r.owner_id = u.id OR EXISTS (
        SELECT 1 FROM user_roles AS ur JOIN resource_role_grants AS g ON g.role_id = ur.role_id
        WHERE ur.user_id = u.id AND g.resource_id = r.id AND g.action = 'share'
      ))
    )`).bind(now, digest, resourceId, userId).run();
  return result.meta?.changes === 1;
}
