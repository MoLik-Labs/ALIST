// Experimental, unexposed credential vault. Caller must authenticate actorId.
// This is not OAuth, a connection manager, or a production security boundary.
const ID = /^[A-Za-z0-9_-]{1,128}$/;
const BASE64URL = /^[A-Za-z0-9_-]+$/;
const PROVIDER = 'google_drive';
const enc = new TextEncoder();
const validId = value => typeof value === 'string' && ID.test(value);
const validTime = value => Number.isSafeInteger(value) && value >= 0;
const nowSeconds = () => Math.floor(Date.now() / 1000);
const toBase64Url = bytes => btoa(String.fromCharCode(...bytes)).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
function fromBase64Url(value, maxLength) {
  if (typeof value !== 'string' || value.length === 0 || value.length > maxLength || !BASE64URL.test(value)) throw new Error('Invalid vault encoding');
  const input = value.replace(/-/g, '+').replace(/_/g, '/');
  const bytes = Uint8Array.from(atob(input + '='.repeat((4 - input.length % 4) % 4)), ch => ch.charCodeAt(0));
  if (toBase64Url(bytes) !== value) throw new Error('Invalid vault encoding');
  return bytes;
}
async function importKey(secret) {
  const key = fromBase64Url(secret, 43);
  if (key.length !== 32) throw new Error('Invalid vault key');
  return crypto.subtle.importKey('raw', key, 'AES-GCM', false, ['encrypt', 'decrypt']);
}
const aad = (id, owner) => enc.encode(JSON.stringify([1, id, owner, PROVIDER]));
const validField = value => typeof value === 'string' && value.length > 0 && value.length <= 4096 && enc.encode(value).byteLength <= 4096 && !/[\x00-\x1f\x7f]/.test(value);
function checkedCredentials(value) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) return null;
  const { clientId, clientSecret, refreshToken } = value;
  if (![clientId, clientSecret, refreshToken].every(validField)) return null;
  return { clientId, clientSecret, refreshToken };
}

// This grants no sharing rights. Only the enabled record owner can read/delete.
export async function createGoogleConnection(db, secret, actorId, input, now = nowSeconds(), requiredSessionHash = null) {
  const credentials = checkedCredentials(input);
  if (!validId(actorId) || !validTime(now) || !credentials || (requiredSessionHash !== null && (typeof requiredSessionHash !== 'string' || !/^[a-f0-9]{64}$/.test(requiredSessionHash)))) return null;
  const plaintext = enc.encode(JSON.stringify(credentials));
  if (plaintext.byteLength > 32768) return null;
  const key = await importKey(secret);
  const id = toBase64Url(crypto.getRandomValues(new Uint8Array(16)));
  const nonce = crypto.getRandomValues(new Uint8Array(12));
  const ciphertext = new Uint8Array(await crypto.subtle.encrypt({ name: 'AES-GCM', iv: nonce, additionalData: aad(id, actorId) }, key, plaintext));
  const guard = requiredSessionHash === null ? '' : ` AND EXISTS (
    SELECT 1 FROM sessions AS s WHERE s.token_hash = ? AND s.user_id = users.id
      AND s.revoked_at IS NULL AND s.expires_at > ?)`;
  const params = [id, PROVIDER, toBase64Url(nonce), toBase64Url(ciphertext), now, actorId];
  if (requiredSessionHash !== null) params.push(requiredSessionHash, now);
  const result = await db.prepare(`INSERT INTO provider_connections (id,owner_id,provider,vault_version,nonce,ciphertext,created_at)
    SELECT ?,id,?,1,?,?,? FROM users WHERE id = ? AND disabled = 0${guard}`).bind(...params).run();
  return result.meta?.changes === 1 ? id : null;
}

export async function readGoogleConnection(db, secret, actorId, id) {
  if (!validId(actorId) || !validId(id)) return null;
  const row = await db.prepare(`SELECT c.id,c.owner_id,c.provider,c.vault_version,c.nonce,c.ciphertext
    FROM provider_connections AS c JOIN users AS u ON u.id = c.owner_id AND u.disabled = 0
    WHERE c.id = ? AND c.owner_id = ? AND c.provider = ? LIMIT 1`).bind(id, actorId, PROVIDER).first();
  if (!row) return null;
  try {
    if (row.vault_version !== 1) throw new Error('Unknown vault version');
    const key = await importKey(secret);
    const nonce = fromBase64Url(row.nonce, 16);
    if (nonce.length !== 12) throw new Error('Invalid nonce');
    const ciphertext = fromBase64Url(row.ciphertext, 45000);
    const plaintext = await crypto.subtle.decrypt({ name: 'AES-GCM', iv: nonce, additionalData: aad(row.id, row.owner_id) }, key, ciphertext);
    const credentials = checkedCredentials(JSON.parse(new TextDecoder('utf-8', { fatal: true }).decode(plaintext)));
    if (!credentials) throw new Error('Invalid credentials');
    return credentials;
  } catch {
    // Do not reveal whether the key, row or encrypted payload caused the error.
    throw new Error('Unable to open provider credentials');
  }
}

export async function deleteGoogleConnection(db, actorId, id, requiredSessionHash = null, now = nowSeconds()) {
  if (!validId(actorId) || !validId(id) || !validTime(now) || (requiredSessionHash !== null && (typeof requiredSessionHash !== 'string' || !/^[a-f0-9]{64}$/.test(requiredSessionHash)))) return false;
  // The session predicate and deletion run as one statement to prevent a
  // session-revocation race between authorization and the destructive write.
  const guard = requiredSessionHash === null ? '' : ` AND EXISTS (
    SELECT 1 FROM sessions AS s WHERE s.token_hash = ? AND s.user_id = provider_connections.owner_id
      AND s.revoked_at IS NULL AND s.expires_at > ?)`;
  const values = [id, actorId, PROVIDER, actorId];
  if (requiredSessionHash !== null) values.push(requiredSessionHash, now);
  const result = await db.prepare(`DELETE FROM provider_connections WHERE id = ? AND owner_id = ? AND provider = ?
    AND EXISTS (SELECT 1 FROM users WHERE id = ? AND disabled = 0)${guard}`).bind(...values).run();
  return result.meta?.changes === 1;
}
