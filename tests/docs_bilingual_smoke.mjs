import { access, readFile } from 'node:fs/promises';
import path from 'node:path';
import assert from 'node:assert/strict';
import { Window } from 'happy-dom';

const dist = path.resolve('docs/starlight/dist');
const repo = (process.env.GITHUB_REPOSITORY || '').split('/')[1] || '';
const base = process.env.DOCS_BASE || (repo && !repo.toLowerCase().endsWith('.github.io') ? `/${repo}/` : '/');
const pages = ['start', 'deploy', 'security', 'monitor-backup', 'cloudflare', 'cloudflare-preview', 'cloudflare-isolated', 'publish-docs'];
const source = process.env.DOCS_REPOSITORY || process.env.GITHUB_REPOSITORY;
for (const [locale, lang] of [['', 'en-US'], ['vi/', 'vi-VN']]) {
  for (const page of ['', ...pages]) {
    const route = `${locale}${page ? `${page}/` : ''}`;
    const html = await readFile(path.join(dist, route, 'index.html'), 'utf8');
    assert.ok(html.includes(`lang="${lang}"`), `${route}: document language`);
    assert.ok(html.includes(`${base}_astro/`), `${route}: configured asset base`);
    assert.ok(html.includes('Kova Docs'), `${route}: site title`);
    assert.ok(html.includes('pagefind'), `${route}: local search`);
    if (source) assert.ok(html.includes(`https://github.com/${source}`), `${route}: fork repository link`);
    if (source && source !== 'MoLik-Labs/Kova-INC') assert.ok(!html.includes('https://github.com/MoLik-Labs/Kova-INC'), `${route}: no upstream link`);
    const window = new Window({ url: `https://example.org${base}${route}` });
    window.document.write(html);
    for (const link of window.document.querySelectorAll('a[href]')) {
      const url = new URL(link.getAttribute('href'), window.location.href);
      if (url.origin !== 'https://example.org') continue;
      assert.ok(url.pathname.startsWith(base), `${route}: link escapes base ${url.pathname}`);
      const rel = decodeURIComponent(url.pathname.slice(base.length));
      const target = path.join(dist, rel, url.pathname.endsWith('/') ? 'index.html' : '');
      await access(target).catch(() => { throw new Error(`${route}: missing link ${url.pathname}`); });
    }
    window.close();
    if (page) {
      const legacy = await readFile(path.join(dist, locale, `${page}.html`), 'utf8');
      assert.ok(legacy.includes(`${base}${locale}${page}/`), `${route}: old URL redirects`);
    }
  }
}
const en = await readFile(path.join(dist, 'cloudflare/index.html'), 'utf8');
const vi = await readFile(path.join(dist, 'vi/cloudflare/index.html'), 'utf8');
assert.match(en, /experimental R2 reader/);
assert.match(vi, /R2 thử nghiệm/);
assert.match(await readFile(path.join(dist, 'vi/deploy/index.html'), 'utf8'), /không có ổ đĩa bền vững/);
await access(path.join(dist, 'pagefind/pagefind.js'));
await access(path.join(dist, 'favicon.svg'));
console.log('Bilingual Starlight pages, links, search assets, legacy redirects and base paths passed');

// Exercise the built Pagefind index via HTTP rather than only checking files.
// Browser interaction still requires a real-browser test.
const { createServer } = await import('node:http');
const { pathToFileURL } = await import('node:url');
const server = createServer(async (req, res) => {
  try {
    const name = new URL(req.url, 'http://localhost').pathname;
    if (!name.startsWith('/pagefind/') || name.includes('..')) { res.writeHead(404).end(); return; }
    const data = await readFile(path.join(dist, name));
    res.setHeader('Content-Type', name.endsWith('.json') ? 'application/json' : 'application/octet-stream');
    res.end(data);
  } catch { res.writeHead(404).end(); }
});
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
try {
  const url = `http://127.0.0.1:${server.address().port}`;
  const pagefind = await import(pathToFileURL(path.join(dist, 'pagefind/pagefind.js')).href);
  await pagefind.options({ basePath: `${url}/pagefind/`, baseUrl: `${url}/` });
  const hits = await pagefind.search('Render');
  assert.ok(hits.results.length > 0, 'search should find Render');
  assert.match((await hits.results[0].data()).url, /deploy/);
  const index = JSON.parse(await readFile(path.join(dist, 'pagefind/pagefind-entry.json'), 'utf8'));
  assert.ok(index.languages['en-us']?.page_count && index.languages['vi-vn']?.page_count, 'both language search indexes');
  console.log('Pagefind HTTP search and bilingual index passed');
} finally {
  server.closeAllConnections();
  await new Promise(resolve => server.close(resolve));
}
