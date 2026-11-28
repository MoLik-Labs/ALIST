// docs/guide remains the single source of truth. Generate Starlight frontmatter
// and preserve VitePress warning semantics without checking in duplicate content.
import { mkdir, readFile, rm, writeFile } from 'node:fs/promises';
import path from 'node:path';

const pages = ['start', 'deploy', 'security', 'monitor-backup', 'cloudflare', 'cloudflare-preview', 'cloudflare-isolated', 'publish-docs'];
const output = 'docs/starlight/src/content/docs';
await rm(output, { recursive: true, force: true });
for (const locale of ['', 'vi']) {
  const dest = path.join(output, locale);
  await mkdir(dest, { recursive: true });
  for (const page of pages) {
    const source = await readFile(path.join('docs/guide', locale, `${page}.md`), 'utf8');
    const match = source.match(/^# (.+)\r?\n/);
    if (!match) throw new Error(`Missing title in ${locale}/${page}.md`);
    let body = source.slice(match[0].length);
    body = body.replace(/^::: (danger|warning) (.+)$/gm, (_, kind, title) => `:::${kind === 'warning' ? 'caution' : 'danger'}[${title}]`);
    // Each output page is a directory route: ../ preserves sibling links.
    body = body.replace(/\]\(\.\/([a-z-]+)(#[^)]+)?\)/g, ']\(../$1/$2)');
    await writeFile(path.join(dest, `${page}.md`), `---\ntitle: ${JSON.stringify(match[1])}\n---\n${body}`);
  }
  const srcHome = await readFile(path.join('docs/guide', locale, 'index.md'), 'utf8');
  const warning = srcHome.slice(srcHome.indexOf('\n---\n', 4) + 5).trim()
    .replace(/^::: warning (.+)$/gm, ':::caution[$1]');
  const home = locale === 'vi'
    ? `---\ntitle: Tệp của bạn. Không gian của bạn.\ndescription: Hướng dẫn triển khai và vận hành Kova.\n---\n\nHướng dẫn thực tế để triển khai, bảo vệ và vận hành kho tệp riêng.\n\n## Bắt đầu từ đâu?\n\n- [Chọn cách triển khai](./start/) — tìm hiểu server chạy ở đâu và dữ liệu được lưu thế nào.\n- [Render và container](./deploy/) — chuẩn bị môi trường, storage và kiểm tra sau triển khai.\n- [Bảo mật và khôi phục](./security/) — bảo vệ tài khoản và thử khôi phục bản sao lưu.\n- [Cloudflare](./cloudflare/) — phân biệt proxy và backend thử nghiệm.\n\n`
    : `---\ntitle: Your files. Your workspace.\ndescription: Practical guides to deploying and operating Kova.\n---\n\nA practical guide to deploying, protecting and operating your own file workspace.\n\n## Where would you like to start?\n\n- [Choose a deployment](./start/) — understand where the server runs and where data lives.\n- [Render and containers](./deploy/) — prepare your environment, storage and deployment checks.\n- [Security and recovery](./security/) — protect accounts and test your backups.\n- [Cloudflare](./cloudflare/) — understand the difference between a proxy and the experimental backend.\n\n`;
  await writeFile(path.join(dest, 'index.md'), home + warning + '\n');
}
