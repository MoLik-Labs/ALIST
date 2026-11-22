// Disabled-by-default, single-account Dropbox experiment; not a multi-user API.
// Only fixed Dropbox hosts are contacted. Never log upstream bodies or tokens.
const API = 'https://api.dropboxapi.com/2/files/';
const CONTENT = 'https://content.dropboxapi.com/2/files/download';
const MAX_JSON = 1024 * 1024;
const json = (status, body) => new Response(JSON.stringify(body), {status, headers:{'content-type':'application/json; charset=utf-8','cache-control':'no-store','x-content-type-options':'nosniff'}});
const fail = (status, message) => json(status,{error:message});
const clean = (s, max) => typeof s === 'string' && s.length <= max && !/[\x00-\x1f\x7f]/.test(s);
const pathOK = s => clean(s, 2000) && (s === '' || (s.startsWith('/') && !s.endsWith('/') && !s.slice(1).split('/').some(part => part === '.' || part === '..' || part === '')));

async function readJson(response) {
  if (!response.body) throw Error('Missing body');
  const reader = response.body.getReader(), chunks = [];
  let total = 0;
  try {
    for (;;) {
      const {value, done} = await reader.read();
      if (done) break;
      total += value.byteLength;
      if (total > MAX_JSON) { await reader.cancel().catch(() => {}); throw Error('Too large'); }
      chunks.push(value);
    }
  } finally { reader.releaseLock(); }
  const data = new Uint8Array(total);
  let offset = 0;
  for (const chunk of chunks) { data.set(chunk, offset); offset += chunk.length; }
  return JSON.parse(new TextDecoder('utf-8', {fatal:true}).decode(data));
}

// Dropbox-API-Arg is an HTTP header and therefore must be ASCII even for
// Unicode filenames. JSON \u escapes are decoded by Dropbox's JSON parser.
function asciiJson(value) {
  return JSON.stringify(value).replace(/[\u007f-\uffff]/g, c => `\\u${c.charCodeAt(0).toString(16).padStart(4,'0')}`);
}

export async function dropboxRequest(url, env) {
  if (env?.KOVA_EXPERIMENTAL_DROPBOX !== 'enabled') return fail(404,'Not found');
  const list = url.pathname === '/v1/dropbox/files', download = url.pathname === '/v1/dropbox/file';
  if (!list && !download) return fail(404,'Not found');
  const token = env.DROPBOX_ACCESS_TOKEN;
  if (typeof token !== 'string' || token.length < 1 || token.length > 4096 || /[^\x21-\x7e]/.test(token)) return fail(503,'Dropbox is not configured');
  const paths = url.searchParams.getAll('path'), cursors = url.searchParams.getAll('cursor');
  if (paths.length > 1 || cursors.length > 1 || (download && (paths.length !== 1 || cursors.length)) ||
      (list && cursors.length && paths.length) || (cursors.length && (!cursors[0] || !clean(cursors[0],4096))) ||
      (paths.length && !pathOK(paths[0])) || (download && !paths[0])) return fail(400,'Invalid request');
  try {
    const headers = {authorization:`Bearer ${token}`};
    let response;
    if (list) {
      headers['content-type'] = 'application/json';
      const cursor = cursors[0];
      const endpoint = cursor ? 'list_folder/continue' : 'list_folder';
      const body = cursor ? {cursor} : {path:paths[0] || '',recursive:false,include_deleted:false,limit:100};
      response = await fetch(API+endpoint,{method:'POST',redirect:'manual',signal:AbortSignal.timeout(30000),headers,body:JSON.stringify(body)});
    } else {
      headers['dropbox-api-arg'] = asciiJson({path:paths[0]});
      const controller = new AbortController(), timer = setTimeout(() => controller.abort(),30000);
      try { response = await fetch(CONTENT,{method:'POST',redirect:'manual',signal:controller.signal,headers}); }
      finally { clearTimeout(timer); }
    }
    if (response.status === 429 || response.status === 503) return fail(503,'Dropbox temporarily unavailable');
    if (response.status !== 200) return fail(502,'Dropbox request failed');
    if (list) {
      const data = await readJson(response);
      if (!Array.isArray(data.entries) || typeof data.has_more !== 'boolean' || !clean(data.cursor,4096) || !data.cursor || data.entries.length > 2000) throw Error('Invalid listing');
      const entries = data.entries.map(entry => {
        if (!entry || !['file','folder'].includes(entry['.tag']) || !clean(entry.name,2000) || !pathOK(entry.path_display) || !entry.path_display) throw Error('Invalid entry');
        const item = {type:entry['.tag'],name:entry.name,path:entry.path_display};
        if (item.type === 'file') {
          if (!Number.isSafeInteger(entry.size) || entry.size < 0) throw Error('Invalid size');
          item.size = entry.size;
        }
        return item;
      });
      return json(200,{entries,nextCursor:data.has_more ? data.cursor : null});
    }
    const headersOut = {'content-type':'application/octet-stream','content-disposition':'attachment','cache-control':'private, no-store','x-content-type-options':'nosniff'};
    const length = response.headers.get('content-length');
    if (length && /^\d{1,19}$/.test(length)) headersOut['content-length'] = length;
    return new Response(response.body,{status:200,headers:headersOut});
  } catch { return fail(502,'Dropbox request failed'); }
}

// Bounded, create-only upload for an authenticated owner. The caller must
// validate the session and decrypted connection; authorize again after reading
// the body so revocation during a slow upload prevents the outbound write.
export async function uploadDropbox(request, url, token, authorize) {
  const paths=url.searchParams.getAll('path');
  if (paths.length !== 1 || !pathOK(paths[0]) || !paths[0] || url.searchParams.has('cursor')) return fail(400,'Invalid request');
  if (typeof authorize !== 'function') return fail(503,'Dropbox unavailable');
  const lengthText=request.headers.get('content-length');
  if (!lengthText || !/^(0|[1-9][0-9]{0,7})$/.test(lengthText)) return fail(411,'Valid Content-Length required');
  const length=Number(lengthText), max=8*1024*1024;
  if (length>max) return fail(413,'Upload too large');
  if (!request.body && length!==0) return fail(400,'Invalid body length');
  try {
    const chunks=[];let total=0;
    if (request.body) {
      const reader=request.body.getReader();
      try {
        for (;;) {
          const {done,value}=await reader.read();if(done)break;
          total+=value.byteLength;
          if(total>length || total>max){await reader.cancel().catch(()=>{});return fail(413,'Upload exceeds declared length or limit');}
          chunks.push(value);
        }
      } finally {reader.releaseLock();}
    }
    if(total!==length)return fail(400,'Invalid body length');
    if(!await authorize())return fail(401,'Unauthorized');
    const bytes=new Uint8Array(total);let offset=0;
    for(const chunk of chunks){bytes.set(chunk,offset);offset+=chunk.byteLength;}
    const response=await fetch('https://content.dropboxapi.com/2/files/upload',{
      method:'POST',redirect:'manual',signal:AbortSignal.timeout(30000),
      headers:{authorization:`Bearer ${token}`,'content-type':'application/octet-stream','dropbox-api-arg':asciiJson({path:paths[0],mode:'add',autorename:false,mute:false,strict_conflict:true})},body:bytes,
    });
    if(response.status===409)return fail(409,'Dropbox could not create the file');
    if(response.status===429 || response.status===503)return fail(503,'Dropbox temporarily unavailable');
    if(response.status!==200)return fail(502,'Dropbox upload failed');
    const data=await readJson(response);
    if(!data || !clean(data.name,2000) || !data.name || !pathOK(data.path_display) || !data.path_display || !Number.isSafeInteger(data.size) || data.size!==total)throw Error('Invalid metadata');
    return json(201,{name:data.name,path:data.path_display,size:data.size});
  } catch {return fail(502,'Dropbox upload failed');}
}
