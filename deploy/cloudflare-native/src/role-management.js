// Isolated D1 role-grant operations. Caller must authenticate actorId first.
const ID = /^[A-Za-z0-9_-]{1,128}$/;
const ACTIONS = new Set(['read', 'write', 'delete', 'share']);
const validId = id => typeof id === 'string' && ID.test(id);
const validTime = n => Number.isSafeInteger(n) && n >= 0;
const nowSeconds = () => Math.floor(Date.now() / 1000);

// Only the enabled owner may revoke a resource's role grant. On removing a
// share grant, revoke affected users' existing links in the same D1 batch so
// links cannot reactivate if the grant is later restored. This deliberately
// revokes even if another role would also permit the user to share.
export async function removeResourceRoleGrant(db, actorId, resourceId, roleId, action, now = nowSeconds()) {
  if (![actorId, resourceId, roleId].every(validId) || !ACTIONS.has(action) || !validTime(now)) return false;
  const statements = [];
  if (action === 'share') {
    statements.push(db.prepare(`UPDATE bearer_shares SET revoked_at = ?
      WHERE resource_id = ? AND revoked_at IS NULL
      AND created_by IN (SELECT user_id FROM user_roles WHERE role_id = ?)
      AND EXISTS (SELECT 1 FROM resource_role_grants WHERE resource_id = ? AND role_id = ? AND action = 'share')
      AND EXISTS (SELECT 1 FROM resources AS r JOIN users AS u ON u.id = r.owner_id AND u.disabled = 0
        WHERE r.id = ? AND r.owner_id = ? AND r.deleted_at IS NULL)`)
      .bind(now, resourceId, roleId, resourceId, roleId, resourceId, actorId));
  }
  statements.push(db.prepare(`DELETE FROM resource_role_grants WHERE resource_id = ? AND role_id = ? AND action = ?
    AND EXISTS (SELECT 1 FROM resources AS r JOIN users AS u ON u.id = r.owner_id AND u.disabled = 0
      WHERE r.id = ? AND r.owner_id = ? AND r.deleted_at IS NULL)`)
    .bind(resourceId, roleId, action, resourceId, actorId));
  const result = await db.batch(statements);
  return result.at(-1)?.meta?.changes === 1;
}
