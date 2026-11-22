import assert from 'node:assert/strict';
import { DatabaseSync } from 'node:sqlite';
import { readFile } from 'node:fs/promises';
import { handleRequest } from '../deploy/cloudflare-native/src/index.js';
import { authenticateSession } from '../deploy/cloudflare-native/src/sessions.js';
const sqlite = new DatabaseSync(':memory:');
for (const name of ['0001_access_foundation.sql','0002_sessions.sql','0006_identity.sql']) sqlite.exec(await readFile(new URL(`../deploy/cloudflare-native/migrations/${name}`,import.meta.url),'utf8'));
let beforeIssuance = null;
const db = {prepare(sql){return {bind(...values){return {
  async first(){return sqlite.prepare(sql).get(...values) ?? null;},
  async run(){if (beforeIssuance && sql.includes('INSERT INTO sessions')) { const f=beforeIssuance; beforeIssuance=null; f(); } return {meta:{changes:Number(sqlite.prepare(sql).run(...values).changes)}};},
};}};}};
sqlite.exec("INSERT INTO users(id,username) VALUES('alice','alice'); INSERT INTO external_identities VALUES('google','google-subject','alice')");
const pair = await crypto.subtle.generateKey({name:'RSASSA-PKCS1-v1_5',modulusLength:2048,publicExponent:new Uint8Array([1,0,1]),hash:'SHA-256'},true,['sign','verify']);
const pub = await crypto.subtle.exportKey('jwk',pair.publicKey); Object.assign(pub,{kid:'key',use:'sig',alg:'RS256'});
const key = Buffer.alloc(32,4).toString('base64url');
const env = {DB:db,KOVA_CREDENTIAL_KEY:key,KOVA_EXPERIMENTAL_V2:'enabled',KOVA_EXPERIMENTAL_LOGIN:'enabled',GOOGLE_CLIENT_ID:'client',GOOGLE_CLIENT_SECRET:'secret',KOVA_LOGIN_REDIRECT:'https://worker.test/v2/auth/google/callback'};
let authParams, subject='google-subject', changeNonce=false, requests=[];
const original = globalThis.fetch;
const b64 = value => Buffer.from(JSON.stringify(value)).toString('base64url');
globalThis.fetch = async (url, options) => {
  requests.push({url:String(url),options});
  if (String(url) === 'https://www.googleapis.com/oauth2/v3/certs') return Response.json({keys:[pub]});
  if (String(url) !== 'https://oauth2.googleapis.com/token') throw Error('unexpected network');
  const verifier=options.body.get('code_verifier');
  const challenge=Buffer.from(await crypto.subtle.digest('SHA-256',new TextEncoder().encode(verifier))).toString('base64url');
  assert.equal(challenge,authParams.get('code_challenge'));
  const now=Math.floor(Date.now()/1000);
  const input=`${b64({alg:'RS256',kid:'key'})}.${b64({iss:'https://accounts.google.com',aud:'client',sub:subject,nonce:changeNonce?'wrong':authParams.get('nonce'),iat:now,exp:now+300})}`;
  const sig=await crypto.subtle.sign('RSASSA-PKCS1-v1_5',pair.privateKey,new TextEncoder().encode(input));
  return Response.json({id_token:`${input}.${Buffer.from(sig).toString('base64url')}`});
};
const start = (origin='https://worker.test') => new Request('https://worker.test/v2/auth/google/start',{method:'POST',headers:{origin,'content-type':'application/json'}});
const callback = (state,cookie) => new Request(`https://worker.test/v2/auth/google/callback?state=${encodeURIComponent(state)}&code=synthetic-code`,{headers:cookie?{cookie}:{}});
async function begin() {
  const r=await handleRequest(start(),env); assert.equal(r.status,200);
  const cookie=r.headers.get('set-cookie'); assert.match(cookie,/^__Host-kova_login=.*; Path=\/; Max-Age=600; Secure; HttpOnly; SameSite=Lax$/);
  authParams=new URL((await r.json()).authorizationUrl).searchParams;
  return {state:authParams.get('state'),cookie:cookie.split(';')[0]};
}
try {
  assert.equal((await handleRequest(start(),{...env,KOVA_EXPERIMENTAL_LOGIN:undefined})).status,404);
  assert.equal((await handleRequest(start('https://evil.test'),env)).status,403);
  assert.equal((await handleRequest(new Request('https://worker.test/v2/auth/google/start',{method:'POST',headers:{origin:'https://worker.test','content-type':'text/plain'}}),env)).status,403);
  assert.equal((await handleRequest(start(),{...env,KOVA_LOGIN_REDIRECT:'https://evil.test/v2/auth/google/callback'})).status,503);
  let {state,cookie}=await begin();
  assert.equal((await handleRequest(callback(state,null),env)).status,400);
  assert.equal((await handleRequest(callback(state,'__Host-kova_login=invalid'),env)).status,400);
  assert.equal(requests.length,0);
  const result=await handleRequest(callback(state,cookie),env);
  assert.equal(result.status,200);
  assert.equal(result.headers.get('cache-control'),'no-store');
  assert.match(result.headers.get('set-cookie'),/Max-Age=0/);
  const session=(await result.json()).token;
  assert.equal(await authenticateSession(db,session),'alice');
  assert.equal((await handleRequest(callback(state,cookie),env)).status,400);
  assert.equal(requests.length,2);
  assert.ok(!JSON.stringify(sqlite.prepare('SELECT * FROM sessions').all()).includes(session));
  ({state,cookie}=await begin());
  const browserEnv={...env,KOVA_EXPERIMENTAL_BROWSER_AUTH:'enabled'};
  const browserResult=await handleRequest(callback(state,cookie),browserEnv);
  assert.equal(browserResult.status,303); assert.equal(browserResult.headers.get('location'),'/');
  assert.equal(await browserResult.text(),'');
  const setCookies=browserResult.headers.getSetCookie();
  assert.equal(setCookies.length,2);
  const sessionCookie=setCookies.find(x=>x.startsWith('__Host-kova_session='));
  assert.match(sessionCookie,/; Secure; HttpOnly; SameSite=Lax$/);
  const browserCookie=sessionCookie.split(';')[0];
  assert.equal((await handleRequest(new Request('https://worker.test/v2/me',{headers:{cookie:browserCookie}}),browserEnv)).status,200);
  assert.equal((await handleRequest(new Request('https://worker.test/v2/logout',{method:'POST',headers:{cookie:browserCookie,origin:'https://evil.test'}}),browserEnv)).status,401);
  const logout=await handleRequest(new Request('https://worker.test/v2/logout',{method:'POST',headers:{cookie:browserCookie,origin:'https://worker.test'}}),browserEnv);
  assert.equal(logout.status,200); assert.match(logout.headers.get('set-cookie'),/Max-Age=0/);
  assert.equal((await handleRequest(new Request('https://worker.test/v2/me',{headers:{cookie:browserCookie}}),browserEnv)).status,401);
  ({state,cookie}=await begin()); subject='unmapped';
  assert.equal((await handleRequest(callback(state,cookie),env)).status,403);
  ({state,cookie}=await begin()); subject='google-subject'; changeNonce=true;
  assert.equal((await handleRequest(callback(state,cookie),env)).status,401);
  ({state,cookie}=await begin()); changeNonce=false;
  sqlite.exec("UPDATE users SET disabled=1 WHERE id='alice'");
  assert.equal((await handleRequest(callback(state,cookie),env)).status,403);
  sqlite.exec("UPDATE users SET disabled=0 WHERE id='alice'");
  ({state,cookie}=await begin());
  beforeIssuance=()=>sqlite.exec("DELETE FROM external_identities WHERE provider='google' AND subject='google-subject'");
  assert.equal((await handleRequest(callback(state,cookie),env)).status,403, 'removed identity must not create a session');
  console.log('Experimental Google login isolation tests passed');
} finally {globalThis.fetch=original; sqlite.close();}
