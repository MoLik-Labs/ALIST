import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { Miniflare, convertV4MiniflareOptions } from '../deploy/cloudflare-worker/node_modules/miniflare/dist/src/index.js';
const code = await readFile(new URL('../deploy/cloudflare-native/src/google-identity.js',import.meta.url),'utf8');
const pair = await crypto.subtle.generateKey({name:'RSASSA-PKCS1-v1_5',modulusLength:2048,publicExponent:new Uint8Array([1,0,1]),hash:'SHA-256'},true,['sign','verify']);
const jwk = await crypto.subtle.exportKey('jwk',pair.publicKey);
Object.assign(jwk,{kid:'test-key',alg:'RS256',use:'sig'});
const nonce = Buffer.alloc(32,3).toString('base64url');
const payload = {iss:'https://accounts.google.com',aud:'client',sub:'subject',nonce,iat:1000,exp:1300};
const enc = value => Buffer.from(JSON.stringify(value)).toString('base64url');
const input = `${enc({alg:'RS256',kid:'test-key'})}.${enc(payload)}`;
const signature = await crypto.subtle.sign('RSASSA-PKCS1-v1_5',pair.privateKey,new TextEncoder().encode(input));
const token = `${input}.${Buffer.from(signature).toString('base64url')}`;
const script = `${code}\nglobalThis.fetch = async (url,options) => {
 if (String(url) !== 'https://www.googleapis.com/oauth2/v3/certs' || options.redirect !== 'manual') throw Error('unexpected network');
 return Response.json({keys:[${JSON.stringify(jwk)}]});
};
export default { async fetch() { return Response.json({verified:await verifyGoogleIdentityToken(${JSON.stringify(token)},'client',${JSON.stringify(nonce)},1100), rejected:await verifyGoogleIdentityToken(${JSON.stringify(token)},'wrong',${JSON.stringify(nonce)},1100)}); } };`;
const mf = new Miniflare(convertV4MiniflareOptions({modules:true,script,compatibilityDate:'2026-11-16'}));
try {
  const response = await mf.dispatchFetch('http://localhost/test');
  assert.equal(response.status,200);
  assert.deepEqual(await response.json(),{verified:'subject',rejected:null});
  console.log('Experimental Google identity workerd tests passed');
} finally { await mf.dispose(); }
