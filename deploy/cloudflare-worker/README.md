# Kova Cloudflare Worker (edge proxy)

This Worker is a **reverse proxy to an already-running Kova origin**. It does not run the Kova server, SQLite database, storage drivers, or embedded UI on Cloudflare. Keep the origin running on Render, a VPS, or another container host with persistent storage, then point the Worker at its public HTTPS origin.

## Deploy

[![Deploy Cloudflare edge proxy](https://deploy.workers.cloudflare.com/button)](https://deploy.workers.cloudflare.com/?url=https%3A%2F%2Fgithub.com%2FMoLik-Labs%2FALIST%2Ftree%2FBETA%2Fdeploy%2Fcloudflare-worker)

The button requires a public repository and deploys only this proxy. It uses
the `BETA` branch. You still need a
running HTTPS origin and the `KOVA_ORIGIN` secret; without it the Worker returns
503. The button flow has not been verified against a real Cloudflare account.


1. Deploy the main Kova container and confirm its `/ping` endpoint works. Keep `/opt/alist/data` on persistent storage; it contains the SQLite database, signing key, and account/storage configuration.
2. Install the Worker dependencies and authenticate Wrangler:

   ```sh
   cd deploy/cloudflare-worker
   npm ci
   npx wrangler login
   ```

3. Set the origin as an encrypted Worker secret. Use only the HTTPS origin (scheme and host, optional port); do not include a path, credentials, query, or fragment:

   ```sh
   npx wrangler secret put KOVA_ORIGIN
   # Example: https://kova.example.com
   ```

4. Deploy:

   ```sh
   npm run deploy
   ```

Wrangler publishes to `workers.dev` because `workers_dev = true`. Configure a Worker Custom Domain or route in Cloudflare if you want a branded hostname. Set `ALIST_SITE_URL` on the origin to that public hostname when Kova needs to generate absolute links.

## Local development and checks

For `npm run dev`, create an untracked `.dev.vars` file in this directory:

```dotenv
KOVA_ORIGIN=https://your-kova-origin.example
```

Then run:

```sh
npm test
npm run dev
```

The repository CI also bundles the Worker with `wrangler deploy --dry-run`. Do not commit `.dev.vars` or origin credentials.

## Behavior and trust boundaries

- The Worker streams request and response bodies, forwards query strings and Range headers, removes hop-by-hop headers, replaces client-supplied forwarding headers, and keeps same-origin redirects on the edge hostname.
- It forces `private, no-store` response caching and disables the Worker fetch cache. Authenticated workspace content, share quota responses, and signed links are not cached at the edge.
- `KOVA_ORIGIN` must be HTTPS. Worker-generated 502/503 responses do not expose upstream exception details.
- The origin is still a separately reachable server unless its network/access policy says otherwise. The Worker alone does **not** protect the origin. Restrict direct origin access where the hosting platform allows it; only set `ALIST_TRUST_PROXY=true` when clients cannot bypass the trusted proxy, because otherwise a direct caller could spoof forwarded client IP headers.
- Cloudflare's request-size, timeout, and runtime limits still apply. Verify large uploads and downloads against the limits of the Cloudflare plan and the chosen origin. The origin must also have persistent storage for production use.

For a durable single-host deployment with persistent SQLite storage, use the root Docker/Render setup instead of treating this proxy as a standalone Worker app.
