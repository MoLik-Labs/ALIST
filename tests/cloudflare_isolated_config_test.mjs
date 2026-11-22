import assert from 'node:assert/strict';
import {readFile,writeFile,unlink,mkdtemp,rm} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import {join} from 'node:path';
import {randomUUID} from 'node:crypto';
import {spawnSync} from 'node:child_process';
const base=new URL('../deploy/cloudflare-native/',import.meta.url);
const example=await readFile(new URL('wrangler.isolated.example.toml',base),'utf8');
assert.match(example,/REPLACE_WITH_NEW_TEST_WORKER_NAME/);
assert.match(example,/REPLACE_WITH_TEST_DATABASE_NAME/);
assert.match(example,/REPLACE_WITH_TEST_DATABASE_ID/);
assert.match(example,/run_worker_first\s*=\s*true/);
assert.doesNotMatch(example,/\[\[r2_buckets\]\]/);
assert.doesNotMatch(example,/KOVA_EXPERIMENTAL_[A-Z_]+\s*=/);
const name=`wrangler.dryrun-${randomUUID()}.toml`, path=new URL(name,base);
const persist=await mkdtemp(join(tmpdir(),'kova-d1-local-'));
try {
  await writeFile(path,example.replace('REPLACE_WITH_NEW_TEST_WORKER_NAME','kova-isolated-ci-test').replace('REPLACE_WITH_TEST_DATABASE_NAME','kova-ci-test').replace('REPLACE_WITH_TEST_DATABASE_ID',randomUUID()),{flag:'wx'});
  const cli=new URL('../deploy/cloudflare-worker/node_modules/.bin/wrangler',import.meta.url);
  const result=spawnSync(cli.pathname,['deploy','--dry-run','--config',path.pathname],{encoding:'utf8',timeout:60000,env:{...process.env,WRANGLER_SEND_METRICS:'false'}});
  assert.equal(result.status,0,`Wrangler dry run failed: ${result.stderr}\n${result.stdout}`);
  assert.match(result.stdout,/env\.DB/);assert.match(result.stdout,/env\.ASSETS/);
  assert.match(result.stdout,/dry-run/);
  const migration=spawnSync(cli.pathname,['d1','migrations','apply','DB','--local','--force','--persist-to',persist,'--config',path.pathname],{encoding:'utf8',timeout:60000,env:{...process.env,WRANGLER_SEND_METRICS:'false'}});
  assert.equal(migration.status,0,`Local migrations failed: ${migration.stderr}\n${migration.stdout}`);
  assert.match(migration.stdout,/0009_google_oauth_browser_binding\.sql/);
  console.log('Isolated Cloudflare template and local D1 migrations passed (no remote changes)');
} finally {await unlink(path).catch(()=>{});await rm(persist,{recursive:true,force:true});}
