import assert from 'node:assert/strict';
import {DatabaseSync} from 'node:sqlite';
import {readFile} from 'node:fs/promises';
import {handleRequest} from '../deploy/cloudflare-native/src/index.js';
import {issueSession, hashSessionToken} from '../deploy/cloudflare-native/src/sessions.js';
const sqlite = new DatabaseSync(':memory:');
for (const name of ['0001_access_foundation.sql','0002_sessions.sql','0007_dropbox_connections.sql']) sqlite.exec(await readFile(new URL('../deploy/cloudflare-native/migrations/'+name,import.meta.url),'utf8'));
sqlite.exec("INSERT INTO users(id,username) VALUES('alice','alice'),('bob','bob')");
let beforeWrite;
const db={prepare(sql){return {bind(...v){return {
  async first(){return sqlite.prepare(sql).get(...v) ?? null;},
  async all(){return {results:sqlite.prepare(sql).all(...v)};},
  async run(){if (beforeWrite && /(?:INSERT INTO|DELETE FROM) dropbox_connections/.test(sql)) {const hook=beforeWrite;beforeWrite=null;hook();} return {meta:{changes:Number(sqlite.prepare(sql).run(...v).changes)}};},
};}};}};
const alice=await issueSession(db,'alice',Math.floor(Date.now()/1000)+600), bob=await issueSession(db,'bob',Math.floor(Date.now()/1000)+600);
const env={DB:db,KOVA_CREDENTIAL_KEY:Buffer.alloc(32,4).toString('base64url'),KOVA_EXPERIMENTAL_V2:'enabled',KOVA_EXPERIMENTAL_DROPBOX_V2:'enabled'};
const make=(who,path,method='GET',body) => new Request('https://worker.test'+path,{method,headers:{authorization:`Bearer ${who}`,...(body ? {'content-type':'application/json'} : {})},...(body ? {body:JSON.stringify(body)} : {})});
const original=globalThis.fetch; let calls=0;
globalThis.fetch=async (url,options)=>{calls++; assert.equal(options.headers.authorization,'Bearer alice-dropbox-token'); return Response.json({entries:[{'.tag':'file',name:'a',path_display:'/a',size:3}],cursor:'next',has_more:false});};
try {
  assert.equal((await handleRequest(make(alice,'/v2/dropbox/connections'),{...env,KOVA_EXPERIMENTAL_DROPBOX_V2:undefined})).status,404);
  assert.equal((await handleRequest(make('bad','/v2/dropbox/connections'),env)).status,401);
  assert.equal((await handleRequest(make(alice,'/v2/dropbox/connections','POST',{accessToken:'bad\nvalue'}),env)).status,400);
  const racing=await issueSession(db,'alice',Math.floor(Date.now()/1000)+600), hash=await hashSessionToken(racing);
  beforeWrite=()=>sqlite.prepare('UPDATE sessions SET revoked_at=? WHERE token_hash=?').run(Math.floor(Date.now()/1000),hash);
  assert.equal((await handleRequest(make(racing,'/v2/dropbox/connections','POST',{accessToken:'secret'}),env)).status,409);
  assert.equal(sqlite.prepare('SELECT count(*) AS n FROM dropbox_connections').get().n,0);
  let response=await handleRequest(make(alice,'/v2/dropbox/connections','POST',{accessToken:'alice-dropbox-token'}),env);
  assert.equal(response.status,201); const {id}=await response.json();
  const row=sqlite.prepare('SELECT * FROM dropbox_connections WHERE id=?').get(id);
  assert.ok(!JSON.stringify(row).includes('alice-dropbox-token'));
  assert.equal((await (await handleRequest(make(bob,'/v2/dropbox/connections'),env)).json()).connections.length,0);
  const listed=await (await handleRequest(make(alice,'/v2/dropbox/connections'),env)).json();
  assert.equal(listed.connections[0].id,id); assert.ok(!JSON.stringify(listed).includes('token'));
  assert.equal((await handleRequest(make(bob,`/v2/dropbox/files?connection=${id}`),env)).status,404); assert.equal(calls,0);
  response=await handleRequest(make(alice,`/v2/dropbox/files?connection=${id}`),env);
  assert.equal(response.status,200); assert.equal((await response.json()).entries[0].name,'a'); assert.equal(calls,1);
  assert.equal((await handleRequest(make(alice,`/v2/dropbox/files?connection=${id}&connection=${id}`),env)).status,400);
  assert.equal((await handleRequest(make(alice,`/v2/dropbox/files?connection=${id}`),{...env,KOVA_CREDENTIAL_KEY:Buffer.alloc(32,5).toString('base64url')})).status,503);
  sqlite.prepare('UPDATE dropbox_connections SET ciphertext=? WHERE id=?').run(row.ciphertext.slice(0,-1)+'!',id);
  assert.equal((await handleRequest(make(alice,`/v2/dropbox/files?connection=${id}`),env)).status,503);
  sqlite.prepare('UPDATE dropbox_connections SET ciphertext=? WHERE id=?').run(row.ciphertext,id);
  const insert=sqlite.prepare("INSERT INTO dropbox_connections VALUES (?,'alice','nonce','cipher',1)");
  for (let i=0;i<49;i++) insert.run('other'+String(i).padStart(17,'0'));
  assert.equal((await handleRequest(make(alice,'/v2/dropbox/connections','POST',{accessToken:'secret'}),env)).status,409);
  assert.equal((await (await handleRequest(make(alice,'/v2/dropbox/connections'),env)).json()).connections.length,50);
  sqlite.prepare("DELETE FROM dropbox_connections WHERE id LIKE 'other%'").run();
  assert.equal((await handleRequest(make(bob,`/v2/dropbox/connections?id=${id}`,'DELETE'),env)).status,404);
  const race2=await issueSession(db,'alice',Math.floor(Date.now()/1000)+600), hash2=await hashSessionToken(race2);
  beforeWrite=()=>sqlite.prepare('UPDATE sessions SET revoked_at=? WHERE token_hash=?').run(Math.floor(Date.now()/1000),hash2);
  assert.equal((await handleRequest(make(race2,`/v2/dropbox/connections?id=${id}`,'DELETE'),env)).status,404);
  assert.ok(sqlite.prepare('SELECT id FROM dropbox_connections WHERE id=?').get(id));
  response=await handleRequest(make(alice,`/v2/dropbox/connections?id=${id}`,'DELETE'),env);
  assert.deepEqual(await response.json(),{ok:true,providerRevoked:false});
  assert.equal((await handleRequest(make(alice,`/v2/dropbox/files?connection=${id}`),env)).status,404);
  console.log('Experimental owner-scoped Dropbox tests passed');
} finally {globalThis.fetch=original;sqlite.close();}
