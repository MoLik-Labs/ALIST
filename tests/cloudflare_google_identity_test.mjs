import assert from 'node:assert/strict';
import { verifyGoogleIdentityToken } from '../deploy/cloudflare-native/src/google-identity.js';
const pair = await crypto.subtle.generateKey({ name:'RSASSA-PKCS1-v1_5', modulusLength:2048, publicExponent:new Uint8Array([1,0,1]), hash:'SHA-256' },true,['sign','verify']);
const pub = await crypto.subtle.exportKey('jwk',pair.publicKey);
Object.assign(pub,{kid:'test-key',alg:'RS256',use:'sig'});
const originalFetch = globalThis.fetch;
let requests = [], mode = 'valid';
globalThis.fetch = async (url, options) => {
  requests.push({ url:String(url), options });
  if (mode === 'redirect') return new Response(null,{status:302,headers:{location:'https://evil.test'}});
  if (mode === 'duplicate') return Response.json({keys:[pub,pub]});
  if (mode === 'oversized') return new Response('x'.repeat(65537));
  return Response.json({keys:[pub]});
};
const nonce = Buffer.alloc(32,3).toString('base64url');
const claims = {iss:'https://accounts.google.com',aud:'client',sub:'google-subject',nonce,iat:1000,exp:1300};
const b64 = value => Buffer.from(typeof value==='string' ? value : JSON.stringify(value)).toString('base64url');
async function sign(payload=claims, header={alg:'RS256',kid:'test-key',typ:'JWT'}) {
  const data = `${b64(header)}.${b64(payload)}`;
  const sig = await crypto.subtle.sign('RSASSA-PKCS1-v1_5',pair.privateKey,new TextEncoder().encode(data));
  return `${data}.${Buffer.from(sig).toString('base64url')}`;
}
try {
  const token = await sign();
  assert.equal(await verifyGoogleIdentityToken(token,'client',nonce,1100),'google-subject');
  assert.equal(requests[0].url,'https://www.googleapis.com/oauth2/v3/certs');
  assert.equal(requests[0].options.redirect,'manual');
  assert.equal(await verifyGoogleIdentityToken(token,'other',nonce,1100),null);
  assert.equal(await verifyGoogleIdentityToken(token,'client',Buffer.alloc(32,4).toString('base64url'),1100),null);
  assert.equal(await verifyGoogleIdentityToken(token,'client',nonce,1300),null);
  for (const patch of [{iss:'https://evil.test'}, {sub:''}, {iat:1200}, {iat:100}, {exp:1000}, {nbf:1200}, {azp:'other'}, {aud:['client']}]) {
    assert.equal(await verifyGoogleIdentityToken(await sign({...claims,...patch}),'client',nonce,1100),null);
  }
  for (const patch of [{alg:'none'}, {kid:'../../evil'}, {jku:'https://evil.test'}, {crit:['exp']}, {typ:'not-jwt'}]) {
    assert.equal(await verifyGoogleIdentityToken(await sign(claims,{alg:'RS256',kid:'test-key',...patch}),'client',nonce,1100),null);
  }
  const pieces = token.split('.');
  assert.equal(await verifyGoogleIdentityToken(`${pieces[0]}.${pieces[1]}.${'A'.repeat(pieces[2].length)}`,'client',nonce,1100),null);
  mode='duplicate'; assert.equal(await verifyGoogleIdentityToken(token,'client',nonce,1100),null);
  mode='redirect'; assert.equal(await verifyGoogleIdentityToken(token,'client',nonce,1100),null);
  mode='oversized'; assert.equal(await verifyGoogleIdentityToken(token,'client',nonce,1100),null);
  assert.equal(await verifyGoogleIdentityToken('garbage','client',nonce,1100),null);
  console.log('Experimental Google identity token verification tests passed');
} finally { globalThis.fetch = originalFetch; }
