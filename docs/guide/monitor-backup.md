# Uptime and backups

## Before enabling anything

`/ping` only verifies the server returns `pong`, not login, database, drives or backups. Console → Overview has an on-demand browser check. After deploying the separate monitor, enter its `https://<worker>.<subdomain>.workers.dev` origin in the monitor section and connect; Kova displays its samples and refreshes each minute while the page is open. The origin is stored in that browser, not shared; clear it and connect to stop. The server fetches only the fixed status path over HTTPS and forwards no credentials. Custom monitor domains are not supported.

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

One monitor uses about 288 KV writes/day, against an account-wide Free quota. Public page visits and an open console consume reads/requests (about one per minute per browser); watch usage. KV is eventually consistent; concurrent or missed invocations may lose samples. Successful samples divided by samples is not an SLA or exact uptime; data older than 15 minutes is marked stale. The 75-second timeout allows for cold starts. Only around 24 hours of samples are kept; no alerts exist. Actual Cron, KV and Render integration has not been verified and neither availability nor free usage is guaranteed.

## NodeWarden and backup

NodeWarden has its own scheduled WebDAV/S3 backups. Kova supports WebDAV, but **real NodeWarden end-to-end compatibility is unverified**. The real NodeWarden WebDAV uploader at commit `954bd05c10535e5828e43103caa821514c333f7c` has been tested locally with dummy uploads, listings, downloads, existence checks and deletion. The test rewrites a test hostname to loopback; it does not verify TLS, a deployed Worker or vault restoration. For further testing use a dedicated restricted Kova user with WebDAV rights only to a separate durable mount, connect over HTTPS at `/dav` using NodeWarden's settings, then test backup and restoration with dummy data in an isolated environment first. Kova requires an `X-OTP-Code` on every WebDAV Basic request for accounts with 2FA; the inspected NodeWarden WebDAV client sends Basic auth only, so those accounts cannot currently connect directly. A dedicated account without TOTP is a security trade-off: restrict its scope and credentials, and do not weaken your administrator account. Never share vault passwords, master keys or backup contents. Do not use Catbox for vault backups: its URLs are public.

The experimental tool below backs up Kova metadata only; a NodeWarden backup does not back up Kova. Render Free loses local SQLite and mount data on spin-down/restart and is unsuitable as a backup destination. Use durable off-host storage, protect keys and encrypt backups, include metadata and attachments, and regularly test recovery. A raw copy of a running SQLite database can omit WAL state; see [security and recovery](./security). Do not automatically delete old backups until a new one has been verified.


## Encrypted Kova metadata backup (experimental; durable host only)

The optional `tools/backup/kova_backup.py` uses SQLite's Backup API (including WAL), encrypts with AES-256-GCM, and verifies decryption and SQLite integrity after each scheduled run. It includes **only** `data.db` and `config.json`, with limits of 64 MiB database and 1 MiB config. Local or cloud-mounted files, NodeWarden, attachments, environment variables, the backup key, and any environment-supplied `ALIST_JWT_SECRET` are excluded. If the server uses that override, protect and recover the exact secret separately or encrypted credentials cannot be recovered. `verify` is not a substitute for restoring and testing login.

This standalone tool requires Python 3.11+ with `sqlite3` and `cryptography`. It is **not included in the Render image**, can upload encrypted snapshots to a configured WebDAV server, and cannot run while its host sleeps. It cannot make Render Free durable. Use it only on a host that can read the live database and write to separate durable storage. Store encrypted backups off-host and keep a recovery copy of the key somewhere **separate**; losing the key makes the backups unreadable. Never commit a key or put it on Catbox or in the backup directory. Backup and restore-parent directories must be private, owned by the executing user and mode 0700. During verify/restore, a **plaintext SQLite scratch file** is written under `TMPDIR` and deleted; use a private encrypted temp filesystem and understand that deletion may not erase underlying blocks.

From a clone with these changes, replace `/path-to-data` with the real host path (do not enter vault passwords):

```sh
python3 -m venv .venv
.venv/bin/pip install -r tools/backup/requirements.txt
mkdir -p "$HOME/kova-keys" "$HOME/kova-backups" "$HOME/kova-temp"
chmod 700 "$HOME/kova-keys" "$HOME/kova-backups" "$HOME/kova-temp"
export TMPDIR="$HOME/kova-temp"
.venv/bin/python tools/backup/kova_backup.py create-key "$HOME/kova-keys/backup.key"
.venv/bin/python tools/backup/kova_backup.py create /path-to-data "$HOME/kova-backups/first.kvbak" "$HOME/kova-keys/backup.key"
.venv/bin/python tools/backup/kova_backup.py verify "$HOME/kova-backups/first.kvbak" "$HOME/kova-keys/backup.key"
.venv/bin/python tools/backup/kova_backup.py restore "$HOME/kova-backups/first.kvbak" "$HOME/kova-keys/backup.key" "$HOME/kova-temp/restore-test"
```

`create-key`, `create`, and `restore` refuse to overwrite; never replace your key. `restore-test` must not already exist. Restored configuration points DB/temp/log to the new directory, but mounts may still target real data: **isolate network and storage before starting the restored instance**. Test login, mounts, and recovery of files and external secrets. Copy ciphertext off-host and verify that a separately stored key can be recovered.

To run daily while the host is running, run this foreground process under your host's systemd/supervisor (not installed or restarted automatically):

```sh
.venv/bin/python tools/backup/kova_backup.py schedule /path-to-data "$HOME/kova-backups" "$HOME/kova-keys/backup.key" --interval 86400
```

It runs immediately and then at the chosen interval in seconds (minimum 3600). Failures go to logs; there are no external alerts or automatic deletion. Monitor free space and logs; choose retention manually only after validating a newer backup. A custom DB path outside the data directory or a config changed during snapshot is rejected. Relative DB paths are interpreted against the tool's working directory: run it from the same working directory as the server. This is experimental and has not been tested with real user data or cloud services.

### Send encrypted snapshots to your WebDAV

Use an **existing** WebDAV directory over HTTPS with a valid trusted certificate, and a dedicated Basic account with HEAD, PUT and GET rights. The tool does not create directories, follow redirects, accept credentials/query/fragment in URLs, or disable TLS verification. Configure only a destination you trust: this is an operator-run CLI, not a public API or SSRF sandbox. Avoid using an admin account when a path-restricted account is possible. Kova WebDAV accounts with TOTP require a per-request OTP and are not compatible with this tool.

Create a private credentials file outside the repository (never send credentials in chat). The following prompts in the terminal; the password is not echoed and the new file uses mode 0600 and refuses overwrite:

```sh
.venv/bin/python - <<'PY'
import getpass, json, os
path = os.path.expanduser('~/kova-keys/webdav.json')
user = input('WebDAV username: ')
password = getpass.getpass('WebDAV password: ')
fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
with os.fdopen(fd, 'w') as f:
    json.dump({'username': user, 'password': password}, f)
PY
```

Replace the example URL with **your** WebDAV directory URL, without passwords or query parameters. Create a fresh uniquely named backup first, then upload it:

```sh
.venv/bin/python tools/backup/kova_backup.py upload-webdav "$HOME/kova-backups/first.kvbak" "$HOME/kova-keys/backup.key" 'https://your-webdav.example/backup-directory' "$HOME/kova-keys/webdav.json"
```

For scheduled uploads add the following **on the same line** as the `schedule` command above:

```sh
--webdav-url 'https://your-webdav.example/backup-directory' --webdav-credentials "$HOME/kova-keys/webdav.json"
```

Each run uses a fresh random filename. The tool verifies the local encrypted snapshot, checks the destination with HEAD, sends PUT with `If-None-Match: *`, then GETs and compares SHA-256 byte-for-byte. Some servers ignore conditional PUT; HEAD cannot eliminate races, so use a dedicated directory and never let clients write the same name concurrently. On upload or verification failure the local backup remains; old files are never deleted and the tool does not automatically fetch remote backups. For recovery, download ciphertext using a secure WebDAV client, then run `verify` and `restore` with the separate key. This flow has been tested with local TLS and dummy data, **not with your WebDAV server**. Do not share credentials, their file or your key with us.
