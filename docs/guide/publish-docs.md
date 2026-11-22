# Publish this documentation

This site uses the MIT-licensed [VitePress](https://github.com/vuejs/vitepress) default theme. It is separate from the Kova application: GitHub Pages serves static documentation, not the API or your files.

1. Fork the repository. Under **Settings → Pages**, select **GitHub Actions** as the build and deployment source. Ensure Actions and Pages are permitted for your repository.
2. In **Actions**, open **Deploy documentation to GitHub Pages** and select **Run workflow**, choosing the branch containing this workflow. Alternatively, changes to the documentation on `BETA` or `main` trigger it automatically. Approve any environment protection rules if configured.
3. Open the completed deployment's URL. Check English and Vietnamese using the language menu, including navigation and search. A green build alone does not prove the published site works.
4. A project site normally lives under `https://<owner>.github.io/<repository>/`; the build detects that subpath. For a custom domain or other base, set the repository Actions variable `DOCS_BASE` under **Settings → Secrets and variables → Actions → Variables**, for example `/` or `/manual/`, then rerun the workflow. The value must start and end with `/`. Remove it to restore automatic detection.

If assets fail to load, check the actual published URL, `DOCS_BASE`, and the workflow logs. GitHub Pages must be enabled and may require permission or environment approval. Do not put credentials or private content in this public static site.

For local commands and development-server security notes see [`docs/guide/README.md`](https://github.com/MoLik-Labs/Kova-INC/blob/BETA/docs/guide/README.md). The Pages deployment has not yet been verified on a real account.

## Cloudflare `assets.directory` does not exist

If `npm run docs:build` succeeds but Wrangler looks for `docs/.vitepress/dist`, the output path is wrong: this repository's VitePress output is **`docs/guide/.vitepress/dist`**, relative to the repository root. `Detected Project Settings` and `Create wrangler.jsonc` in the log indicate Wrangler inferred temporary settings, not the Kova backend configuration. The `npm audit` warning is not the cause of the missing directory.

To publish the **documentation** on Cloudflare, create a separate Pages project (not the `kova-inc` application Worker), with repository root, build command `DOCS_BASE=/ npm run docs:build`, and build output directory `docs/guide/.vitepress/dist`. Do not publish the whole repository. Verify the URL and CSS/JS afterward. If hosting docs under a subdirectory, use that path as `DOCS_BASE` instead of `/`. This is a static site, not the Kova application or backend.

Do not retry an assets-only `wrangler deploy` against the existing application Worker: it can replace Worker code and configuration/bindings. If the goal is to fix the `kova-inc` application, disable its incorrectly configured automatic build and inspect its purpose, configuration, bindings, secrets and current deployment before changing it; correcting the documentation path does not fix the application backend or UI. A real Cloudflare deployment still needs verification in your account.
