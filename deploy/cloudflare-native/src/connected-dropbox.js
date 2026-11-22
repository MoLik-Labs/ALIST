// Isolated, opt-in Dropbox access-token connections. Not production-ready:
// no OAuth refresh, rate limiting, provider revocation or frontend integration.
import {authenticateSession, hashSessionToken} from './sessions.js';
import {vaultKey, decode, readSmallJson} from './google-oauth.js';
import {dropboxRequest} from './dropbox.js';
const enc = new TextEncoder();
const b64 = bytes => btoa(String.fromCharCode(...bytes)).replace(/\+/g,'-').replace(/\//g,'_').replace(/=+$/,'');
const idPattern = /^[A-Za-z0-9_-]{22}$/;
const bearer = /^Bearer ([A-Za-z0-9_-]{43})$/;
const validToken = v => typeof v === 'string' && v.length > 0 && v.length <= 4096 && /^[\x21-\x7e]+$/.test(v);
const aad = (id, user) => enc.encode(JSON.stringify([1,'dropbox',id,user]));
const now = () => Math.floor(Date.now()/1000);
const json = (status, value) => new Response(JSON.stringify(value),{status,headers:{'content-type':'application/json; charset=utf-8','cache-control':'no-store','x-content-type-options':'nosniff','referrer-policy':'no-referrer'}});
const fail = (status, message) => json(status,{error:message});

export async function connectedDropboxRequest(request, env) {
  if (env?.KOVA_EXPERIMENTAL_V2 !== 'enabled' || env?.KOVA_EXPERIMENTAL_DROPBOX_V2 !== 'enabled') return fail(404,'Not found');
  const url = new URL(request.url), connections = url.pathname === '/v2/dropbox/connections';
  const drive = url.pathname === '/v2/dropbox/files' || url.pathname === '/v2/dropbox/file';
  if (!connections && !drive) return fail(404,'Not found');
  if ((drive && request.method !== 'GET') || (connections && !['GET','POST','DELETE'].includes(request.method))) return fail(405,'Method not allowed');
  if (!env.DB || typeof env.KOVA_CREDENTIAL_KEY !== 'string') return fail(503,'Experimental backend is not configured');
  const session = bearer.exec(request.headers.get('authorization') || '')?.[1];
  if (!session) return fail(401,'Unauthorized');
  try {
    const user = await authenticateSession(env.DB, session);
    if (!user) return fail(401,'Unauthorized');
    if (connections && request.method === 'POST') {
      if (!/^application\/json(?:\s*;|$)/i.test(request.headers.get('content-type') || '')) return fail(415,'Expected JSON');
      let body;
      try { body = await readSmallJson(request); } catch { return fail(400,'Invalid request'); }
      if (!body || typeof body !== 'object' || Array.isArray(body) || Object.keys(body).length !== 1 || !validToken(body.accessToken)) return fail(400,'Invalid request');
      const key = await vaultKey(env.KOVA_CREDENTIAL_KEY), id = b64(crypto.getRandomValues(new Uint8Array(16)));
      const nonce = crypto.getRandomValues(new Uint8Array(12));
      const cipher = new Uint8Array(await crypto.subtle.encrypt({name:'AES-GCM',iv:nonce,additionalData:aad(id,user)},key,enc.encode(body.accessToken)));
      const timestamp = now(), sessionHash = await hashSessionToken(session);
      const result = await env.DB.prepare(`INSERT INTO dropbox_connections (id,owner_id,nonce,ciphertext,created_at)
        SELECT ?,u.id,?,?,? FROM users AS u WHERE u.id=? AND u.disabled=0
        AND EXISTS (SELECT 1 FROM sessions s WHERE s.token_hash=? AND s.user_id=u.id AND s.revoked_at IS NULL AND s.expires_at>?)
        AND (SELECT COUNT(*) FROM dropbox_connections WHERE owner_id=u.id)<50`)
        .bind(id,b64(nonce),b64(cipher),timestamp,user,sessionHash,timestamp).run();
      return result.meta?.changes === 1 ? json(201,{id}) : fail(409,'Connection unavailable');
    }
    if (connections && request.method === 'GET') {
      const result = await env.DB.prepare('SELECT id,created_at FROM dropbox_connections WHERE owner_id=? ORDER BY id LIMIT 50').bind(user).all();
      if (!Array.isArray(result.results)) throw Error('Invalid listing');
      return json(200,{connections:result.results.map(row => ({id:row.id,provider:'dropbox',createdAt:row.created_at}))});
    }
    const ids = url.searchParams.getAll(connections ? 'id' : 'connection');
    if (ids.length !== 1 || !idPattern.test(ids[0])) return fail(400,'Invalid request');
    if (connections) {
      const timestamp=now(), hash=await hashSessionToken(session);
      const result=await env.DB.prepare(`DELETE FROM dropbox_connections WHERE id=? AND owner_id=?
        AND EXISTS (SELECT 1 FROM users u WHERE u.id=dropbox_connections.owner_id AND u.disabled=0)
        AND EXISTS (SELECT 1 FROM sessions s WHERE s.token_hash=? AND s.user_id=dropbox_connections.owner_id AND s.revoked_at IS NULL AND s.expires_at>?)`)
        .bind(ids[0],user,hash,timestamp).run();
      return result.meta?.changes === 1 ? json(200,{ok:true,providerRevoked:false}) : fail(404,'Not found');
    }
    const row=await env.DB.prepare(`SELECT c.id,c.owner_id,c.nonce,c.ciphertext FROM dropbox_connections c
      JOIN users u ON u.id=c.owner_id AND u.disabled=0 WHERE c.id=? AND c.owner_id=? LIMIT 1`).bind(ids[0],user).first();
    if (!row) return fail(404,'Not found');
    const key=await vaultKey(env.KOVA_CREDENTIAL_KEY), nonce=decode(row.nonce,16);
    if (nonce.length !== 12) throw Error('Invalid nonce');
    const plain=await crypto.subtle.decrypt({name:'AES-GCM',iv:nonce,additionalData:aad(row.id,row.owner_id)},key,decode(row.ciphertext,6000));
    const accessToken=new TextDecoder('utf-8',{fatal:true}).decode(plain);
    if (!validToken(accessToken)) throw Error('Invalid token');
    const target=new URL(url); target.pathname=url.pathname.replace('/v2/','/v1/'); target.searchParams.delete('connection');
    return await dropboxRequest(target,{KOVA_EXPERIMENTAL_DROPBOX:'enabled',DROPBOX_ACCESS_TOKEN:accessToken});
  } catch { return fail(503,'Experimental backend unavailable'); }
}
