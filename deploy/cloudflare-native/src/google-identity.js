// Isolated experimental OIDC ID-token verifier. This does not implement a
// login flow, nonce storage or replay prevention. Never log tokens or claims.
const JWKS_URL = 'https://www.googleapis.com/oauth2/v3/certs';
const MAX_TOKEN_LENGTH = 16384;
const MAX_JWKS_BYTES = 65536;
const enc = new TextEncoder();
const decoder = new TextDecoder('utf-8', { fatal: true });
const b64 = bytes => btoa(String.fromCharCode(...bytes)).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
function decode(part, max) {
  if (typeof part !== 'string' || part.length === 0 || part.length > max || !/^[A-Za-z0-9_-]+$/.test(part)) throw Error('Invalid token');
  const input = part.replace(/-/g, '+').replace(/_/g, '/');
  const bytes = Uint8Array.from(atob(input + '='.repeat((4-input.length%4)%4)), c => c.charCodeAt(0));
  if (b64(bytes) !== part) throw Error('Invalid token');
  return bytes;
}
async function readJson(response) {
  if (!response.body) throw Error('Missing keys');
  const reader = response.body.getReader(); const chunks = []; let total = 0;
  try {
    while (true) {
      const {done,value} = await reader.read();
      if (done) break;
      total += value.byteLength;
      if (total > MAX_JWKS_BYTES) { await reader.cancel().catch(() => {}); throw Error('Keys too large'); }
      chunks.push(value);
    }
  } finally { reader.releaseLock(); }
  const bytes = new Uint8Array(total); let offset = 0;
  for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.length; }
  return JSON.parse(decoder.decode(bytes));
}

// Returns only the stable Google subject; email must never be used as identity.
// The nonce MUST be independently generated, bound to one auth attempt and
// atomically consumed by the caller. This verifier does not do that itself.
export async function verifyGoogleIdentityToken(token, clientId, nonce, now = Math.floor(Date.now()/1000)) {
  if (typeof token !== 'string' || token.length > MAX_TOKEN_LENGTH || typeof clientId !== 'string' || !clientId || clientId.length > 1024 || typeof nonce !== 'string' || !/^[A-Za-z0-9_-]{43}$/.test(nonce) || !Number.isSafeInteger(now) || now < 0) return null;
  try {
    const parts = token.split('.');
    if (parts.length !== 3) return null;
    const header = JSON.parse(decoder.decode(decode(parts[0], 2048)));
    const claims = JSON.parse(decoder.decode(decode(parts[1], 12000)));
    const signature = decode(parts[2], 2048);
    if (!header || typeof header !== 'object' || header.alg !== 'RS256' || typeof header.kid !== 'string' || !/^[A-Za-z0-9_-]{1,128}$/.test(header.kid) || (header.typ !== undefined && header.typ !== 'JWT') || header.crit !== undefined || header.jku !== undefined || header.x5u !== undefined) return null;
    if (!claims || typeof claims !== 'object' || Array.isArray(claims)) return null;
    if (claims.iss !== 'https://accounts.google.com' && claims.iss !== 'accounts.google.com') return null;
    // Google documents aud as the client ID string. Reject alternate shapes.
    if (claims.aud !== clientId || (claims.azp !== undefined && claims.azp !== clientId)) return null;
    if (typeof claims.sub !== 'string' || !/^[\x21-\x7e]{1,255}$/.test(claims.sub) || claims.nonce !== nonce) return null;
    if (!Number.isSafeInteger(claims.iat) || !Number.isSafeInteger(claims.exp) || claims.iat > now + 60 || claims.iat < now - 600 || claims.exp <= now || claims.exp <= claims.iat) return null;
    if (claims.nbf !== undefined && (!Number.isSafeInteger(claims.nbf) || claims.nbf > now + 60)) return null;
    const response = await fetch(JWKS_URL, { redirect:'manual', signal:AbortSignal.timeout(15000), headers:{ accept:'application/json' } });
    if (response.status !== 200) return null;
    const jwks = await readJson(response);
    if (!Array.isArray(jwks.keys) || jwks.keys.length > 32) return null;
    const matching = jwks.keys.filter(k => k && k.kid === header.kid && k.kty === 'RSA' && k.alg === 'RS256' && k.use === 'sig' && typeof k.n === 'string' && typeof k.e === 'string');
    if (matching.length !== 1) return null;
    const publicKey = await crypto.subtle.importKey('jwk', matching[0], { name:'RSASSA-PKCS1-v1_5', hash:'SHA-256' }, false, ['verify']);
    if (publicKey.algorithm.modulusLength < 2048) return null;
    const valid = await crypto.subtle.verify('RSASSA-PKCS1-v1_5', publicKey, signature, enc.encode(`${parts[0]}.${parts[1]}`));
    return valid ? claims.sub : null;
  } catch { return null; }
}
