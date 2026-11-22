# Uptime and backups

## Before enabling anything

`/ping` only verifies the server returns `pong`, not login, database, drives or backups. Console → Overview has an on-demand browser check; it is not background monitoring and does not retain history.

The optional separate Worker below is inspired by UptimeFlare, not a full port: no notifications, multiple monitors or automatic upgrades. Its status page is **public**. Monitor only a service you own or are authorized to check. Five-minute checks may reduce idle spin-down but **do not guarantee availability**. Render may restart or suspend services for other reasons, and its 750 Free instance hours/month are shared across the workspace. Pinging does not make the filesystem durable. Read [Render Free](https://render.com/docs/free).

## Optional Render monitor (not yet tested in a real account)

1. Once the changes are merged and pushed, clone the correct branch of **your fork**, install Node.js/npm, enter the repo and run `npm ci --ignore-scripts --prefix deploy/cloudflare-worker`.
2. In Cloudflare Workers & Pages check the Free plan, activate workers.dev if needed, and pick a **new unused Worker name**. Never use `kova-inc` or an existing application Worker: a deployment can replace its configuration.
3. Create a **new, separate KV namespace** in Workers KV and record its ID. Do not reuse D1 `kova-cloudflare` or an existing namespace. Review [KV limits](https://developers.cloudflare.com/kv/platform/limits/) and [Workers limits](https://developers.cloudflare.com/workers/platform/limits/); do not upgrade or add a payment method unless you choose to.
4. Copy the example and edit the copy:

```sh
cp deploy/uptime-monitor/wrangler.example.toml deploy/uptime-monitor/wrangler.local.toml
```

Set `name` to the new Worker, `id` to the new KV namespace ID, and `RENDER_ORIGIN` to your own HTTPS **onrender.com** origin, e.g. `https://your-service.onrender.com` without a trailing slash, path or query. The template is intentionally invalid until edited. Do not enter passwords. The local copy is gitignored; do not commit it. Custom domains are not supported in this version.
5. Authenticate with `./deploy/cloudflare-worker/node_modules/.bin/wrangler login`, then dry-run from the repository root:

```sh
./deploy/cloudflare-worker/node_modules/.bin/wrangler deploy --dry-run --config deploy/uptime-monitor/wrangler.local.toml
```

Verify account, Worker name, URL and binding. Dry-run does not create resources. Once certain this is a **new** Worker and separate KV, repeat without `--dry-run` to deploy for real. Do not alter `kova-inc`. Open the Worker URL shown. Cron propagation can take 15 minutes; wait an additional five-minute interval. Disable the cron/Worker when no longer needed.

One monitor uses about 288 KV writes/day, against an account-wide Free quota. Public page visits also consume reads/requests; watch usage. KV is eventually consistent; concurrent or missed invocations may lose samples. Successful samples divided by samples is not an SLA or exact uptime; data older than 15 minutes is marked stale. The 75-second timeout allows for cold starts. Only around 24 hours of samples are kept; no alerts exist. Actual Cron, KV and Render integration has not been verified and neither availability nor free usage is guaranteed.

## NodeWarden and backup

NodeWarden has its own scheduled WebDAV/S3 backups. Kova supports WebDAV, but **real NodeWarden end-to-end compatibility is unverified**. A local synthetic test exercises MKCOL, PUT, HEAD, PROPFIND, GET and DELETE using dummy data. For further testing use a dedicated restricted Kova user with WebDAV rights only to a separate durable mount, connect over HTTPS at `/dav` using NodeWarden's settings, then test backup and restoration with dummy data in an isolated environment first. Kova requires an `X-OTP-Code` on every WebDAV Basic request for accounts with 2FA; the inspected NodeWarden WebDAV client sends Basic auth only, so those accounts cannot currently connect directly. A dedicated account without TOTP is a security trade-off: restrict its scope and credentials, and do not weaken your administrator account. Never share vault passwords, master keys or backup contents. Do not use Catbox for vault backups: its URLs are public.

Kova **does not yet provide secure automated backup/restore** here; a NodeWarden backup does not back up Kova. Render Free loses local SQLite and mount data on spin-down/restart and is unsuitable as a backup destination. Use durable off-host storage, protect keys and encrypt backups, include metadata and attachments, and regularly test recovery. A raw copy of a running SQLite database can omit WAL state; see [security and recovery](./security). Do not automatically delete old backups until a new one has been verified.
