# Try the Cloudflare interface

::: danger Test data only
This interface connects to an independent Cloudflare backend **under development**, not a complete Kova replacement. Uploads, sharing, complete permissions, many drivers and production protections are not ready. Google, Dropbox and real Cloudflare resources have not been verified. Do not use sensitive accounts or files.
:::

## 1. Preview locally

Install Node.js and run from the repository root:

```sh
npm ci --ignore-scripts
npm ci --ignore-scripts --prefix deploy/cloudflare-worker
deploy/cloudflare-worker/node_modules/.bin/wrangler dev --config deploy/cloudflare-native/wrangler.local.toml --local --ip 127.0.0.1 --port 8787 --var KOVA_NATIVE_TOKEN:local-test-secret-at-least-32-characters
```

Open `http://127.0.0.1:8787/` on **the machine running Wrangler**. This is a local configuration with an emulated R2 bucket and a public example token. Stop it with Ctrl+C. Never deploy this local config or use the example token on the Internet. This mode previews the interface only: it has no D1, Google OAuth or user sessions, so sign-in will not work. Run `npm run test:cf-ui` for DOM-emulated interaction and asset tests; emulation is not a real browser test.

## 2. Using the experiment

When a separately configured test backend and pre-provisioned account are available, the Google sign-in button starts an experimental flow. The callback currently returns a JSON session token: a tester must return and enter it manually. The token stays only in tab memory and is lost on reload. **Never send tokens in chat, URLs, logs or screenshots.** This is a temporary process, not suitable for real users.

After sign-in, select a connection to browse folders and use **Load more** for the next page. You can test adding Dropbox using a short-lived token from a test account. The Google Drive connection button opens a separate window; after its callback completes, return and **Refresh**. Removing a connection only removes it from Kova, not from Google or Dropbox; revoke provider access separately. UI downloads are limited to 8 MiB; Google Workspace files request PDF export, which Google may not support for every document type.

## 3. Backend setup and safety

Multi-user experiments require a separate Worker, separate D1 with migrations, separate vault key, configured OAuth and pre-provisioned identities. See the [Cloudflare-native README](https://github.com/MoLik-Labs/Kova-INC/blob/CLOUDFLARE/deploy/cloudflare-native/README.md) for bindings, variables, limitations and risks. Never attach experimental configuration to a production Worker or data. Enable experimental flags only in isolation; v1 uses a shared token with access to the whole provider account and is not a multi-user boundary.

Before real use the project still needs login without copying tokens, abuse protection, key recovery, permissions/sharing, additional drivers, real browser and cloud-resource tests. If CSS or JS does not load, check `GET /`, `/app.css`, `/app.js`, the `ASSETS` binding, build configuration and CSP; do not publish the entire repository.
