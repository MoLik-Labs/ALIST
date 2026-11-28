// Preserve previously published VitePress .html links on static hosts.
import { mkdir, writeFile } from 'node:fs/promises';
import path from 'node:path';
const repo = (process.env.GITHUB_REPOSITORY || '').split('/')[1] || '';
const base = process.env.DOCS_BASE || (repo && !repo.toLowerCase().endsWith('.github.io') ? `/${repo}/` : '/');
if (!/^\/(?:[A-Za-z0-9._~-]+\/)*$/.test(base)) throw new Error('Invalid DOCS_BASE');
for (const locale of ['', 'vi/']) {
  for (const page of ['start', 'deploy', 'security', 'monitor-backup', 'cloudflare', 'cloudflare-preview', 'cloudflare-isolated', 'publish-docs']) {
    const target = `${base}${locale}${page}/`;
    const dir = path.join('docs/starlight/dist', locale);
    await mkdir(dir, { recursive: true });
    await writeFile(path.join(dir, `${page}.html`), `<!doctype html><html lang="${locale ? 'vi' : 'en'}"><head><meta charset="utf-8"><script>location.replace(${JSON.stringify(target)} + location.search + location.hash)</script><meta http-equiv="refresh" content="0; url=${target}"><link rel="canonical" href="${target}"><title>Kova Docs</title></head><body><a href="${target}">Continue to Kova Docs</a></body></html>`);
  }
}
