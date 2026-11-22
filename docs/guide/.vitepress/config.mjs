import { defineConfig } from 'vitepress';

// Project Pages sites live at /<repo>/; account Pages sites live at /.
// Set DOCS_BASE explicitly when deploying to a custom domain or subdirectory.
const repository = process.env.DOCS_REPOSITORY || process.env.GITHUB_REPOSITORY || '';
if (repository && !/^[A-Za-z0-9](?:[A-Za-z0-9-]{0,38})\/[A-Za-z0-9_.-]{1,100}$/.test(repository)) throw new Error('DOCS_REPOSITORY must be a GitHub owner/repository');
const repoUrl = repository ? `https://github.com/${repository}` : '';
const repo = (process.env.GITHUB_REPOSITORY || '').split('/')[1] || '';
const inferredBase = repo && !repo.toLowerCase().endsWith('.github.io') ? `/${repo}/` : '/';
const base = process.env.DOCS_BASE || inferredBase;
if (!/^\/(?:[A-Za-z0-9._~-]+\/)*$/.test(base)) throw new Error('DOCS_BASE must be an absolute path ending in /');

export default defineConfig({
  title: 'Kova Docs',
  description: 'Practical guides for deploying and operating Kova.',
  base,
  cleanUrls: false,
  lastUpdated: false,
  locales: {
    root: {
      label: 'English', lang: 'en-US',
      themeConfig: {
        nav: [{ text: 'Guide', link: '/start' }, ...(repoUrl ? [{ text: 'GitHub', link: repoUrl }] : [])],
        sidebar: [{ text: 'Get started', items: [{ text: 'Choose a deployment', link: '/start' }, { text: 'Render and containers', link: '/deploy' }, { text: 'Cloudflare', link: '/cloudflare' }, { text: 'Cloudflare preview tutorial', link: '/cloudflare-preview' }, { text: 'Isolated Cloudflare test', link: '/cloudflare-isolated' }, { text: 'Uptime and backups', link: '/monitor-backup' }, { text: 'Security and recovery', link: '/security' }, { text: 'Publish documentation', link: '/publish-docs' }] }],
        outline: { label: 'On this page' },
        docFooter: { prev: 'Previous page', next: 'Next page' },
      },
    },
    vi: {
      label: 'Tiếng Việt', lang: 'vi-VN',
      themeConfig: {
        nav: [{ text: 'Hướng dẫn', link: '/vi/start' }, ...(repoUrl ? [{ text: 'GitHub', link: repoUrl }] : [])],
        sidebar: [{ text: 'Bắt đầu', items: [{ text: 'Chọn cách triển khai', link: '/vi/start' }, { text: 'Render và container', link: '/vi/deploy' }, { text: 'Cloudflare', link: '/vi/cloudflare' }, { text: 'Hướng dẫn bản thử nghiệm Cloudflare', link: '/vi/cloudflare-preview' }, { text: 'Triển khai thử nghiệm cô lập', link: '/vi/cloudflare-isolated' }, { text: 'Uptime và sao lưu', link: '/vi/monitor-backup' }, { text: 'Bảo mật và khôi phục', link: '/vi/security' }, { text: 'Xuất bản tài liệu', link: '/vi/publish-docs' }] }],
        outline: { label: 'Trên trang này' },
        docFooter: { prev: 'Trang trước', next: 'Trang tiếp' },
      },
    },
  },
  themeConfig: {
    search: { provider: 'local' },
    socialLinks: repoUrl ? [{ icon: 'github', link: repoUrl }] : [],
    footer: { message: 'Kova · Open source · AGPL-3.0', copyright: 'Documentation is part of the Kova project.' },
  },
});
