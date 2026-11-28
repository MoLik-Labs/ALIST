# Kova documentation site

The source of truth is the English Markdown in this directory and the
Vietnamese Markdown in `vi/`. The static site uses Astro and Starlight, with
self-hosted Pagefind search. Starlight and Nimbus are independent Astro docs
frameworks; this implementation uses Starlight and draws on Nimbus's emphasis
on repo-owned content and validation, not Nimbus's backend features. The old
`docs/site` is a separate legacy site.

## Build locally

Use Node.js 22.23.4 or later.

```sh
npm ci --ignore-scripts
npm run docs:dev
npm run docs:build
npm run test:docs-build
```

The content generation script mirrors the Markdown into an ignored Starlight
content directory, adds frontmatter and maps warnings to Starlight asides.
Edit `docs/guide`, not the generated files. The development server binds to
loopback and is not a production server. Astro telemetry is disabled by the
command wrapper. Local search is generated at build time. Review dependency
advisories with `npm audit` and keep dependencies up to date.

## GitHub Pages

1. In your fork, choose **Settings → Pages → GitHub Actions** as the source.
   Actions and Pages must be permitted; approve environment rules if applicable.
2. Push documentation changes to `BETA` or `main`, or run **Deploy documentation
   to GitHub Pages** from Actions using the branch with this workflow.
3. The workflow builds and publishes `docs/starlight/dist`. Open the URL from
   the deployment, switch languages and check navigation, search and old links.
4. A project site automatically uses `/<repository>/`; an account site uses `/`.
   Override with an Actions variable `DOCS_BASE`, for example `/` for a custom
   domain or `/manual/` for a subdirectory. It must start and end with `/`.
   `DOCS_REPOSITORY` may be set to `owner/repository` to choose the GitHub
   link; otherwise the workflow uses its fork's `GITHUB_REPOSITORY`.

The build creates static redirects from old `.html` page URLs; JavaScript preserves query strings and fragments, with a plain link and meta refresh as a fallback. The site serves
public documentation only; it cannot run the Kova API or store your files.
For Cloudflare docs, create a separate static project: do not overwrite the
existing application Worker. See the bilingual Publish documentation page.
