// Experimental D1 management primitive; not an HTTP handler or login flow.
// Caller must derive actorId from a trusted authentication system, never from
// client input. D1 batch is transactional; independent statements are NOT safe.
const ID = /^[A-Za-z0-9_-]{1,128}$/;
const validId = id => typeof id === 'string' && ID.test(id);
const validTime = n => Number.isSafeInteger(n) && n >= 0;
const nowSeconds = () => Math.floor(Date.now() / 1000);

// All queries repeat the authorization predicate so a failed first update can
// never cause an unauthorized session/share revocation. User-disable, session
// revocation, and link revocation execute in one D1 transactional batch.
const actorHasPrivilege = `EXISTS (
  SELECT 1 FROM users AS a
  JOIN user_roles AS ur ON ur.user_id = a.id
  JOIN role_permissions AS p ON p.role_id = ur.role_id
  WHERE a.id = ? AND a.disabled = 0 AND p.permission = 'manage_users'
)`;

export async function disableUser(db, actorId, targetId, now = nowSeconds()) {
  if (!validId(actorId) || !validId(targetId) || actorId === targetId || !validTime(now)) return false;
  const results = await db.batch([
    db.prepare(`UPDATE users SET disabled = 1 WHERE id = ? AND disabled = 0 AND ${actorHasPrivilege}`)
      .bind(targetId, actorId),
    db.prepare(`UPDATE sessions SET revoked_at = ? WHERE user_id = ? AND revoked_at IS NULL
      AND EXISTS (SELECT 1 FROM users WHERE id = ? AND disabled = 1) AND ${actorHasPrivilege}`)
      .bind(now, targetId, targetId, actorId),
    db.prepare(`UPDATE bearer_shares SET revoked_at = ? WHERE created_by = ? AND revoked_at IS NULL
      AND EXISTS (SELECT 1 FROM users WHERE id = ? AND disabled = 1) AND ${actorHasPrivilege}`)
      .bind(now, targetId, targetId, actorId),
  ]);
  return results[0]?.meta?.changes === 1;
}
