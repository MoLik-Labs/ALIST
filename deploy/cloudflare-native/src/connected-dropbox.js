// Isolated, opt-in Dropbox credential connections. Not production-ready:
// no interactive Dropbox OAuth, rate limiting or provider revocation.
import {authenticateSession, hashSessionToken, sessionFromRequest} from './sessions.js';
import {vaultKey, decode, readSmallJson} from './google-oauth.js';
import {dropboxRequest, uploadDropbox} from './dropbox.js';
const enc = new TextEncoder();
const b64 = bytes => btoa(String.fromCharCode(...bytes)).replace(/\+/g,'-').replace(/\//g,'_').replace(/=+$/,'');
const idPattern = /^[A-Za-z0-9_-]{22}$/;
const validToken = v => typeof v === 'string' && v.length > 0 && v.length <= 4096 && /^[\x21-\x7e]+$/.test(v);
function encodeCredentials(body) {
  if (!body || typeof body !== 'object' || Array.isArray(body)) return null;
  const keys = Object.keys(body).sort().join(',');
  if (keys === 'accessToken' && validToken(body.accessToken)) return body.accessToken; // legacy encoding
  if (keys === 'appKey,appSecret,refreshToken' && [body.appKey,body.appSecret,body.refreshToken].every(validToken)) {
    const payload = '\u0001' + JSON.stringify({appKey:body.appKey,appSecret:body.appSecret,refreshToken:body.refreshToken});
    return enc.encode(payload).byteLength <= 12000 ? payload : null;
  }
  return null;
}
async function accessTokenFor(plaintext) {
  if (!plaintext.startsWith('\u0001')) {
    if (!validToken(plaintext)) throw Error('Invalid token');
    return plaintext;
  }
  const credentials = JSON.parse(plaintext.slice(1));
  if (encodeCredentials(credentials) !== plaintext) throw Error('Invalid credentials');
  // As in the native C++ Dropbox driver, refresh at Dropbox's fixed OAuth host.
  // No cross-user token cache: never trade owner isolation for speed.
  const response = await fetch('https://api.dropboxapi.com/oauth2/token',{
    method:'POST',redirect:'manual',signal:AbortSignal.timeout(15000),
    headers:{'content-type':'application/x-www-form-urlencoded'},
    body:new URLSearchParams({grant_type:'refresh_token',client_id:credentials.appKey,client_secret:credentials.appSecret,refresh_token:credentials.refreshToken}),
  });
  if (response.status !== 200) throw Error('Dropbox authorization failed');
  const accessToken = (await readSmallJson(response)).access_token;
  if (!validToken(accessToken)) throw Error('Invalid access token');
  return accessToken;
}
const aad = (id, user) => enc.encode(JSON.stringify([1,'dropbox',id,user]));
const now = () => Math.floor(Date.now()/1000);
const json = (status, value) => new Response(JSON.stringify(value),{status,headers:{'content-type':'application/json; charset=utf-8','cache-control':'no-store','x-content-type-options':'nosniff','referrer-policy':'no-referrer'}});
const fail = (status, message) => json(status,{error:message});

export async function connectedDropboxRequest(request, env) {
  if (env?.KOVA_EXPERIMENTAL_V2 !== 'enabled' || env?.KOVA_EXPERIMENTAL_DROPBOX_V2 !== 'enabled') return fail(404,'Not found');
  const url = new URL(request.url), connections = url.pathname === '/v2/dropbox/connections';
  const drive = url.pathname === '/v2/dropbox/files' || url.pathname === '/v2/dropbox/file';
  if (!connections && !drive) return fail(404,'Not found');
  const upload = url.pathname === '/v2/dropbox/file' && request.method === 'PUT';
  if ((drive && request.method !== 'GET' && !upload) || (connections && !['GET','POST','DELETE'].includes(request.method))) return fail(405,'Method not allowed');
  if (upload && env.KOVA_EXPERIMENTAL_DROPBOX_WRITES !== 'enabled') return fail(403,'Writes are disabled');
  if (!env.DB || typeof env.KOVA_CREDENTIAL_KEY !== 'string') return fail(503,'Experimental backend is not configured');
  const session = sessionFromRequest(request, env);
  if (!session) return fail(401,'Unauthorized');
  try {
    const user = await authenticateSession(env.DB, session);
    if (!user) return fail(401,'Unauthorized');
    if (connections && request.method === 'POST') {
      if (!/^application\/json(?:\s*;|$)/i.test(request.headers.get('content-type') || '')) return fail(415,'Expected JSON');
      let body;
      try { body = await readSmallJson(request); } catch { return fail(400,'Invalid request'); }
      const credentialText=encodeCredentials(body);
      if (credentialText === null) return fail(400,'Invalid request');
      const key = await vaultKey(env.KOVA_CREDENTIAL_KEY), id = b64(crypto.getRandomValues(new Uint8Array(16)));
      const nonce = crypto.getRandomValues(new Uint8Array(12));
      const cipher = new Uint8Array(await crypto.subtle.encrypt({name:'AES-GCM',iv:nonce,additionalData:aad(id,user)},key,enc.encode(credentialText)));
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
    const plain=await crypto.subtle.decrypt({name:'AES-GCM',iv:nonce,additionalData:aad(row.id,row.owner_id)},key,decode(row.ciphertext,20000));
    const credentialText=new TextDecoder('utf-8',{fatal:true}).decode(plain);
    // Resolve lazily after request validation (and, for uploads, after the
    // live-session recheck), avoiding token refresh for invalid requests.
    const accessToken=()=>accessTokenFor(credentialText);
    if (upload) {
      // Recheck the session, account and unchanged encrypted connection after
      // the request body has been read and just before the provider write.
      const hash=await hashSessionToken(session);
      const guard=async()=>!!(await env.DB.prepare(`SELECT 1 AS ok FROM dropbox_connections c
        JOIN users u ON u.id=c.owner_id AND u.disabled=0
        JOIN sessions s ON s.user_id=u.id AND s.token_hash=? AND s.revoked_at IS NULL AND s.expires_at>?
        WHERE c.id=? AND c.owner_id=? AND c.nonce=? AND c.ciphertext=? /* upload_guard */`)
        .bind(hash,now(),row.id,user,row.nonce,row.ciphertext).first());
      return await uploadDropbox(request,url,accessToken,guard);
    }
    const target=new URL(url); target.pathname=url.pathname.replace('/v2/','/v1/'); target.searchParams.delete('connection');
    return await dropboxRequest(target,{KOVA_EXPERIMENTAL_DROPBOX:'enabled',DROPBOX_ACCESS_TOKEN:accessToken});
  } catch { return fail(503,'Experimental backend unavailable'); }
}
