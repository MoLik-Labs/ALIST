import { defineConfig } from 'astro/config';
import starlight from '@astrojs/starlight';

const repository = process.env.DOCS_REPOSITORY || process.env.GITHUB_REPOSITORY || '';
if (repository && !/^[A-Za-z0-9](?:[A-Za-z0-9-]{0,38})\/[A-Za-z0-9_.-]{1,100}$/.test(repository)) throw new Error('DOCS_REPOSITORY must be a GitHub owner/repository');
const repo = (process.env.GITHUB_REPOSITORY || '').split('/')[1] || '';
const inferredBase = repo && !repo.toLowerCase().endsWith('.github.io') ? `/${repo}/` : '/';
const base = process.env.DOCS_BASE || inferredBase;
if (!/^\/(?:[A-Za-z0-9._~-]+\/)*$/.test(base)) throw new Error('DOCS_BASE must be an absolute path ending in /');

export default defineConfig({
  base,
  trailingSlash: 'always',
  integrations: [starlight({
    title: 'Kova Docs',
    description: 'Practical guides for deploying and operating Kova.',
    defaultLocale: 'root',
    locales: { root: { label: 'English', lang: 'en-US' }, vi: { label: 'Tiếng Việt', lang: 'vi-VN' } },
    social: repository ? [{ icon: 'github', label: 'GitHub', href: `https://github.com/${repository}` }] : [],
    customCss: ['./src/styles/custom.css'],
    sidebar: [
      { label: 'Start here', translations: { 'vi-VN': 'Bắt đầu' }, items: [
        { slug: 'start' }, { slug: 'deploy' }, { slug: 'security' }, { slug: 'monitor-backup' },
      ] },
      { label: 'Cloudflare', items: [
        { slug: 'cloudflare' }, { slug: 'cloudflare-preview' }, { slug: 'cloudflare-isolated' },
      ] },
      { label: 'Documentation', translations: { 'vi-VN': 'Tài liệu' }, items: [{ slug: 'publish-docs' }] },
    ],
  })],
});
