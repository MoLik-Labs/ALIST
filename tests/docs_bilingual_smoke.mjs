import { readFile } from 'node:fs/promises';
import assert from 'node:assert/strict';

const dist = new URL('../docs/guide/.vitepress/dist/', import.meta.url);
const repo = (process.env.GITHUB_REPOSITORY || '').split('/')[1] || '';
const base = process.env.DOCS_BASE || (repo && !repo.toLowerCase().endsWith('.github.io') ? `/${repo}/` : '/');
for (const [path, lang] of [['index.html', 'en-US'], ['start.html', 'en-US'], ['deploy.html', 'en-US'], ['cloudflare.html', 'en-US'], ['cloudflare-preview.html', 'en-US'], ['security.html', 'en-US'], ['publish-docs.html', 'en-US'], ['vi/index.html', 'vi-VN'], ['vi/start.html', 'vi-VN'], ['vi/deploy.html', 'vi-VN'], ['vi/cloudflare.html', 'vi-VN'], ['vi/cloudflare-preview.html', 'vi-VN'], ['vi/security.html', 'vi-VN'], ['vi/publish-docs.html', 'vi-VN']]) {
  const html = await readFile(new URL(path, dist), 'utf8');
  assert.ok(html.includes(`lang="${lang}"`), `${path} must have correct language`);
  assert.ok(html.includes(`${base}assets/`), `${path} must use the configured GitHub Pages base`);
  assert.ok(html.includes('Kova Docs'), `${path} must have a title`);
  const source = process.env.DOCS_REPOSITORY || process.env.GITHUB_REPOSITORY;
  if (source) assert.ok(html.includes(`https://github.com/${source}`), `${path} must link to the build repository`);
  if (source && source !== 'MoLik-Labs/Kova-INC') assert.ok(!html.includes('https://github.com/MoLik-Labs/Kova-INC'), `${path} must not link to the upstream repository`);
}
const en = await readFile(new URL('cloudflare.html', dist), 'utf8');
const vi = await readFile(new URL('vi/cloudflare.html', dist), 'utf8');
assert.match(en, /experimental R2 reader/);
assert.match(vi, /R2 thử nghiệm/);
console.log('Bilingual documentation build and base-path checks passed');
