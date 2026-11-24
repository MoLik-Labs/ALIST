import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { readFile, readdir, lstat } from 'node:fs/promises';
import path from 'node:path';

const lock = JSON.parse(await readFile('skills/sources.lock.json', 'utf8'));
assert.equal(lock.format, 1);
assert.deepEqual(Object.keys(lock.sources).sort(), ['cc-skills-golang', 'ecc', 'impeccable', 'low-level-dev-skills', 'spf13-go-skills', 'superpowers', 'superpowers-marketplace', 'taste', 'ui-ux-pro-max', 'vercel-agent-skills']);
for (const [name, source] of Object.entries(lock.sources)) {
  assert.match(source.commit, /^[0-9a-f]{40}$/);
  const root = path.join('skills', name);
  async function filesAt(dir, prefix = '') {
    const result = [];
    for (const entry of await readdir(dir, { withFileTypes: true })) {
      // Python's ignored bytecode cache is generated when using the search tool.
      if (entry.name === '__pycache__' && entry.isDirectory()) continue;
      const relative = prefix ? `${prefix}/${entry.name}` : entry.name;
      const full = path.join(dir, entry.name);
      const stat = await lstat(full);
      assert.ok(!stat.isSymbolicLink(), `${name}/${relative}: symlinks are not allowed`);
      if (stat.isDirectory()) result.push(...await filesAt(full, relative));
      else if (stat.isFile()) result.push(relative);
      else assert.fail(`${name}/${relative}: unexpected file type`);
    }
    return result;
  }
  const supplemental = new Set(name === 'vercel-agent-skills' ? ['README.md'] : ['LICENSE', ...(name === 'impeccable' ? ['NOTICE.md'] : [])]);
  const actual = (await filesAt(root)).filter(file => !supplemental.has(file)).sort();
  const expected = Object.keys(source.files).sort();
  assert.deepEqual(actual, expected, `${name}: snapshot file list differs from lock`);
  for (const file of expected) {
    const digest = createHash('sha256').update(await readFile(path.join(root, file))).digest('hex');
    assert.equal(digest, source.files[file], `${name}/${file}: checksum mismatch`);
  }
  for (const file of supplemental) assert.ok((await readFile(path.join(root, file))).length > 0, `${name}/${file}: missing license/notice`);
  console.log(`${name}: verified ${expected.length} upstream files at ${source.commit.slice(0, 12)}`);
}
