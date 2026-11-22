// Opt-in Dropbox OAuth connection flow; local/mock tests only.
import {authenticateSession,sessionFromRequest,hashSessionToken} from './sessions.js';
import {vaultKey,decode,readSmallJson} from './google-oauth.js';
import {createDropboxConnection} from './connected-dropbox.js';
const enc=new TextEncoder();
const b64=bytes=>btoa(String.fromCharCode(...bytes)).replace(/\+/g,'-').replace(/\//g,'_').replace(/=+$/,'');
const sha=async value=>new Uint8Array(await crypto.subtle.digest('SHA-256',enc.encode(value)));
const hex=bytes=>[...bytes].map(b=>b.toString(16).padStart(2,'0')).join('');
const rand=()=>b64(crypto.getRandomValues(new Uint8Array(32)));
const pattern=/^[A-Za-z0-9_-]{43}$/;
const valid=x=>typeof x==='string' && x.length>0 && x.length<=4096 && /^[\x21-\x7e]+$/.test(x);
const now=()=>Math.floor(Date.now()/1000);
// Use the __Host- prefix, Secure, no Domain, Path=/ to prevent cookie tossing.
const name='__Host-kova_dropbox_link';
const cookie=(value,maxAge)=>`${name}=${value}; Path=/; Max-Age=${maxAge}; Secure; HttpOnly; SameSite=Lax`;
const json=(status,data,setCookie)=>{const h=new Headers({'content-type':'application/json; charset=utf-8','cache-control':'no-store','x-content-type-options':'nosniff','referrer-policy':'no-referrer','content-security-policy':"default-src 'none'; frame-ancestors 'none'"});if(setCookie)h.append('set-cookie',setCookie);return new Response(JSON.stringify(data),{status,headers:h});};
const fail=(status,message)=>json(status,{error:message});
const aad=(state,user,session,browser)=>enc.encode(JSON.stringify([1,'dropbox-oauth',state,user,session,browser]));
function browserCookie(request){const matches=(request.headers.get('cookie')||'').split(';').map(x=>x.trim()).filter(x=>x.startsWith(name+'='));if(matches.length!==1)return null;const value=matches[0].slice(name.length+1);return pattern.test(value)?value:null;}
function config(env,url){
  if(!env.DB || !valid(env.DROPBOX_CLIENT_ID) || !valid(env.DROPBOX_CLIENT_SECRET) || typeof env.KOVA_CREDENTIAL_KEY!=='string' || typeof env.KOVA_DROPBOX_REDIRECT!=='string')return null;
  try{const target=new URL(env.KOVA_DROPBOX_REDIRECT);if(target.protocol!=='https:' || target.origin!==url.origin || target.username || target.password || target.search || target.hash || target.pathname!=='/v2/dropbox/oauth/callback')return null;return target.toString();}catch{return null;}
}
export async function dropboxOauthRequest(request,env){
  if(env?.KOVA_EXPERIMENTAL_V2!=='enabled' || env?.KOVA_EXPERIMENTAL_DROPBOX_V2!=='enabled' || env?.KOVA_EXPERIMENTAL_DROPBOX_OAUTH!=='enabled')return fail(404,'Not found');
  const url=new URL(request.url),start=url.pathname==='/v2/dropbox/oauth/start',callback=url.pathname==='/v2/dropbox/oauth/callback';
  if(!start && !callback)return fail(404,'Not found');
  if((start && request.method!=='POST') || (callback && request.method!=='GET'))return fail(405,'Method not allowed');
  const redirect=config(env,url);if(!redirect)return fail(503,'OAuth is not configured');
  try{
    const key=await vaultKey(env.KOVA_CREDENTIAL_KEY),time=now();
    if(start){
      const session=sessionFromRequest(request,env);if(!session)return fail(401,'Unauthorized');
      const user=await authenticateSession(env.DB,session,time);if(!user)return fail(401,'Unauthorized');
      const sessionHash=await hashSessionToken(session),state=rand(),verifier=rand(),browser=rand();
      const stateHash=hex(await sha(state)),browserHash=hex(await sha(browser)),iv=crypto.getRandomValues(new Uint8Array(12));
      const ciphertext=new Uint8Array(await crypto.subtle.encrypt({name:'AES-GCM',iv,additionalData:aad(stateHash,user,sessionHash,browserHash)},key,enc.encode(verifier)));
      await env.DB.prepare('DELETE FROM dropbox_oauth_pending WHERE user_id=? AND expires_at<=?').bind(user,time).run();
      const result=await env.DB.prepare(`INSERT INTO dropbox_oauth_pending (state_hash,user_id,session_hash,browser_hash,verifier_nonce,verifier_ciphertext,expires_at,created_at)
        SELECT ?,u.id,s.token_hash,?,?,?,?,? FROM users u JOIN sessions s ON s.user_id=u.id
        WHERE u.id=? AND u.disabled=0 AND s.token_hash=? AND s.revoked_at IS NULL AND s.expires_at>?
        AND (SELECT COUNT(*) FROM dropbox_oauth_pending WHERE user_id=u.id AND expires_at>?)<5`)
        .bind(stateHash,browserHash,b64(iv),b64(ciphertext),time+600,time,user,sessionHash,time,time).run();
      if(result.meta?.changes!==1)return fail(429,'Unable to start another connection');
      const scopes=['files.metadata.read','files.content.read'];if(env.KOVA_EXPERIMENTAL_DROPBOX_OAUTH_WRITE==='enabled')scopes.push('files.content.write');
      const target=new URL('https://www.dropbox.com/oauth2/authorize');target.search=new URLSearchParams({client_id:env.DROPBOX_CLIENT_ID,redirect_uri:redirect,response_type:'code',token_access_type:'offline',scope:scopes.join(' '),state,code_challenge:b64(await sha(verifier)),code_challenge_method:'S256'}).toString();
      return json(200,{authorizationUrl:target.toString()},cookie(browser,600));
    }
    const states=url.searchParams.getAll('state'),codes=url.searchParams.getAll('code'),browser=browserCookie(request);
    if(states.length!==1 || codes.length!==1 || !pattern.test(states[0]) || !valid(codes[0]) || !browser)return fail(400,'Invalid or expired connection request');
    const stateHash=hex(await sha(states[0])),browserHash=hex(await sha(browser));
    const pending=await env.DB.prepare(`DELETE FROM dropbox_oauth_pending WHERE state_hash=? AND browser_hash=? AND expires_at>? AND EXISTS (
      SELECT 1 FROM sessions s JOIN users u ON u.id=s.user_id AND u.disabled=0
      WHERE s.token_hash=dropbox_oauth_pending.session_hash AND s.user_id=dropbox_oauth_pending.user_id AND s.revoked_at IS NULL AND s.expires_at>?)
      RETURNING user_id,session_hash,verifier_nonce,verifier_ciphertext`).bind(stateHash,browserHash,time,time).first();
    if(!pending)return fail(400,'Invalid or expired connection request');
    const iv=decode(pending.verifier_nonce,16);if(iv.length!==12)throw Error('Invalid nonce');
    const verifier=new TextDecoder('utf-8',{fatal:true}).decode(await crypto.subtle.decrypt({name:'AES-GCM',iv,additionalData:aad(stateHash,pending.user_id,pending.session_hash,browserHash)},key,decode(pending.verifier_ciphertext,256)));
    if(!pattern.test(verifier))throw Error('Invalid verifier');
    const result=await fetch('https://api.dropboxapi.com/oauth2/token',{method:'POST',redirect:'manual',signal:AbortSignal.timeout(15000),headers:{'content-type':'application/x-www-form-urlencoded'},body:new URLSearchParams({code:codes[0],client_id:env.DROPBOX_CLIENT_ID,client_secret:env.DROPBOX_CLIENT_SECRET,redirect_uri:redirect,grant_type:'authorization_code',code_verifier:verifier})});
    if(result.status!==200)return fail(502,'Dropbox authorization failed');
    const refreshToken=(await readSmallJson(result)).refresh_token;if(!valid(refreshToken))return fail(502,'Dropbox authorization did not return an offline token');
    const connection=await createDropboxConnection(env.DB,env.KOVA_CREDENTIAL_KEY,pending.user_id,{appKey:env.DROPBOX_CLIENT_ID,appSecret:env.DROPBOX_CLIENT_SECRET,refreshToken},pending.session_hash);
    if(!connection)return fail(409,'Connection unavailable');
    return json(201,{connection},cookie('',0));
  }catch{return fail(503,'Experimental OAuth unavailable');}
}
