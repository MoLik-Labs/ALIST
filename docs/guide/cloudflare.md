# Cloudflare: what runs where?

Cloudflare Workers support JavaScript, TypeScript, Python and Rust, and also WebAssembly for other languages. TypeScript is not the only option. A normal native Linux server cannot simply be deployed as a Worker. A standalone Kova backend would need a separate implementation using Cloudflare runtime APIs and persistent services such as D1 and R2. **Only an experimental R2 reader exists; the complete backend is not implemented.**

The current Worker and Pages projects forward requests to a Kova server you host elsewhere. They do not create a database, accounts, file drivers, or a Kova origin.

## Worker proxy

1. First deploy Kova on a host with a persistent disk. Verify its HTTPS URL, `/ping` and login.
2. Use the Worker deployment button in the repository README, or connect your fork to Workers Builds. For your fork select the correct branch and root directory `deploy/cloudflare-worker`.
3. Set the **runtime secret** `KOVA_ORIGIN` to your server's HTTPS origin, for example `https://example.onrender.com` (no path, query, user or password). Build-time variables alone do not configure runtime secrets.
4. Deploy and open the Worker URL. Test login, listing, upload/download, and restart. A 503 may indicate a missing or invalid origin; a 502 indicates an upstream failure. Check Cloudflare's build and runtime logs without sharing secrets.

## Pages proxy

Create a Pages project linked to your repository and branch. Set root directory to `deploy/cloudflare-pages`, build command to `exit 0`, and output directory to `public`. Set the Pages runtime secret `KOVA_ORIGIN` as above, then redeploy. See the [Pages guide](https://github.com/MoLik-Labs/Kova-INC/tree/BETA/deploy/cloudflare-pages) for full details. Pages Git integration and deployment require verification on your account.

## Unstyled page or inactive buttons?

This can happen when `web/index.html` is deployed as static files without its matching CSS/JS, or when scripts are blocked. The updated source references local assets, but **static UI is not the server**: API requests will fail and the interface displays an unavailable-server warning. Use one of the proxy projects above with an origin instead of publishing `web` as an application.

::: warning Security boundary
The origin remains directly reachable unless you restrict it. Leave `ALIST_TRUST_PROXY` disabled unless untrusted clients cannot bypass the proxy. Disable unsafe caching and account for Cloudflare transfer limits. Use isolated credentials and resources for previews; never connect untrusted previews to production data.
:::
