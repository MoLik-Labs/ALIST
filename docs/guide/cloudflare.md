# Cloudflare: what runs where?

Cloudflare Workers support JavaScript, TypeScript, Python and Rust, and also WebAssembly for other languages. TypeScript is not the only option. A normal native Linux server cannot simply be deployed as a Worker. A standalone Kova backend would need a separate implementation using Cloudflare runtime APIs and persistent services such as D1 and R2. **An experimental R2 reader with opt-in small create-only uploads and a single-account read-only Google Drive connector exist; the complete backend is not implemented. The Google Drive connector is tested with mocks only and is not integrated into the UI.**

The current Worker and Pages projects forward requests to a Kova server you host elsewhere. They do not create a database, accounts, file drivers, or a Kova origin.

## Worker proxy

1. First deploy Kova on a host with a persistent disk. Verify its HTTPS URL, `/ping` and login.
2. Use the Worker deployment button in the repository README, or connect your fork to Workers Builds. For your fork select the correct branch and root directory `deploy/cloudflare-worker`.
3. `KOVA_ORIGIN` is the HTTPS URL of **your separately deployed Kova server**, not a code issued by Cloudflare. For example, after deploying to Render, find the service URL in its dashboard, check `/ping` and login, and use its origin such as `https://example.onrender.com` (no path, query, user or password). Set that as the **runtime secret** `KOVA_ORIGIN`. Build-time variables alone do not configure runtime secrets. If you have no separate server, you do not yet have this value; the standalone Cloudflare backend is not complete.
4. Deploy and open the Worker URL. Test login, listing, upload/download, and restart. A 503 may indicate a missing or invalid origin; a 502 indicates an upstream failure. Check Cloudflare's build and runtime logs without sharing secrets.

## Pages proxy

Create a Pages project linked to your repository and branch. Set root directory to `deploy/cloudflare-pages`, build command to `exit 0`, and output directory to `public`. Set the Pages runtime secret `KOVA_ORIGIN` as above, then redeploy. See the `deploy/cloudflare-pages/README.md` in your repository and branch for full details. Pages Git integration and deployment require verification on your account.

## Unstyled page or inactive buttons?

This can happen when `web/index.html` is deployed as static files without its matching CSS/JS, or when scripts are blocked. The updated source references local assets, but **static UI is not the server**: API requests will fail and the interface displays an unavailable-server warning. Use one of the proxy projects above with an origin instead of publishing `web` as an application.

::: warning Security boundary
The origin remains directly reachable unless you restrict it. Leave `ALIST_TRUST_PROXY` disabled unless untrusted clients cannot bypass the proxy. Disable unsafe caching and account for Cloudflare transfer limits. Use isolated credentials and resources for previews; never connect untrusted previews to production data.
:::

## Workers Builds reports a missing `previews` block

A log showing `Executing user deploy command: npx wrangler preview` and `missing a previews block` is a **Worker preview build failure**, not a Cloudflare Pages error. If previews are not needed, open the Worker → **Settings → Build → Branch control** and disable **Enable Preview Builds**. According to [Cloudflare's guide](https://developers.cloudflare.com/workers/ci-cd/builds/build-branches/), this does not change the production branch. Dashboard labels can differ. Do not replace the preview command with `wrangler deploy`, which could overwrite production.

If previews are needed, first identify the intended Worker and build root directory. Add a `previews` configuration only to the project you intend to deploy, after isolating preview resources and secrets from production. Never attach real data to untrusted previews. `deploy/cloudflare-worker` is a **proxy**; `deploy/cloudflare-native` is still only a prototype. Adding `previews` does not complete the standalone backend. Inspect the next build log: fixing the first error does not guarantee deployment success.

## Free-tier limits

The [official Workers limits](https://developers.cloudflare.com/workers/platform/limits/) include request and CPU allowances for the Free plan; reaching a limit can stop requests. D1, R2 and Google Drive have separate quotas. Unlimited free usage cannot be guaranteed. Check current limits and protect data before creating resources. The Cloudflare-native backend in this repository remains experimental and has not been verified on live services.

## Static-only Worker and wrong build branch

If Settings says runtime variables/bindings cannot be added because the Worker has only static assets, its current deployment has no backend. For example, production branch `BETA`, root `/`, and `npx wrangler deploy` do not automatically select the Cloudflare backend under `deploy/cloudflare-native`. A root-level `npx wrangler preview` does not configure a preview automatically either.

To avoid further accidental deployments while the backend is being prepared, in **Settings → Builds** enable **Disable builds** for Production and disable **Builds for Preview branches** on the preview tab. Do not delete the Worker or replace the preview command with `wrangler deploy`. These changes only stop new builds; they do not repair the current deployment. Create a separate test Worker with isolated D1/R2 and secrets before testing the backend. Code may still be in a PR or another branch, so verify the branch actually contains the commit. Do not deploy the local `wrangler.local.toml` to production.
