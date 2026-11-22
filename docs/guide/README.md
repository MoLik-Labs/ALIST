# Kova documentation site

This site uses the default theme and localization system from
[VitePress](https://github.com/vuejs/vitepress) (MIT license), customized for
Kova. Content is in English at the root and Vietnamese in `vi/`. The old
`docs/site` is a separate legacy static site and is not the source for this one.

## Develop and build

```sh
npm ci --ignore-scripts
npm run docs:dev
npm run docs:build
npm run test:docs-build
```

The development server binds to loopback and is not a production server. The
pinned VitePress 1.6.4 dependency currently brings development-server advisories
in Vite/esbuild; do not expose the dev server to untrusted networks. The built
site is static and does not include a Vite server. Check `npm audit` and update
when a supported stable release addresses these advisories.

## GitHub Pages

1. In your fork's GitHub **Settings → Pages**, choose **GitHub Actions** as the
   source. The repository/account must permit Pages and Actions.
2. Push a change to `BETA` or `main` or run **Deploy documentation to GitHub
   Pages** from the Actions tab. Approve any environment protection rules.
3. The workflow builds `docs/guide/.vitepress/dist` and publishes it as a Pages
   artifact. The URL is shown by the deployment and depends on your account.
4. For `<owner>.github.io/<repository>/`, the workflow detects the repository
   name and builds with that base. For an account site `<owner>.github.io` it
   uses `/`. For a custom domain or other base path, add the repository Actions
   variable `DOCS_BASE` under **Settings → Secrets and variables → Actions →
   Variables** (for example `/` or `/manual/`), then rerun the workflow. It
   must begin and end with `/`. Remove the variable to restore automatic base
   detection. For local builds, set the `DOCS_BASE` environment variable instead.
   No absolute URLs to the original repository's Pages hostname are required.
5. Open both languages, follow the internal links and test on mobile. This site
   hosts documentation only; it does not deploy or run the Kova server.

Publishing requires repository settings and permissions outside this project;
the workflow alone does not enable Pages. Deployment has not been tested on a
live GitHub Pages account.
