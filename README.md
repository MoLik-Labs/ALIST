# Kova Native Workspace

**Kova** is a self-hosted, AList-compatible file workspace powered by a native C++20 server and an embedded, zero-CDN web app. It keeps the historical `alist` executable, CLI options, API routes, and WebDAV endpoint while delivering the Kova EN/VI workspace UI.

- **Version:** `v4.2.0`
- **Engine:** C++20, with streaming I/O and Linux `sendfile` support
- **License:** AGPL-3.0 (see [`LICENSE`](LICENSE))

## What’s included

- A responsive file explorer with list/grid layouts, large-list windowing, keyboard navigation, and FTS5 search/command palette.
- EN/VI language switching, light/dark/system themes, file previews and editing, Markdown rendering, ZIP browsing, and streaming uploads.
- Public share pages at `/s/:id`, with password and expiration/download-limit options.
- Account security features including TOTP two-factor authentication and session management.
- An Admin Console for storage mounts, users, roles/path rules, metadata rules, shares, tasks, system settings, and telemetry.
- Built-in Local, S3, WebDAV, Google Drive, OneDrive, Dropbox, SFTP, FTP, URLTree, Alias, and Crypt storage drivers.
- Compatible server surfaces: `/api/*`, `/dav/*`, `/d/*`, `/p/*`, and the existing `alist` CLI.

The browser UI is embedded in the native binary. Runtime assets are served locally; the application does not need a third-party CDN.

The bilingual documentation website is built with [VitePress](https://github.com/vuejs/vitepress) (MIT) from [`docs/guide`](docs/guide/). Run `npm ci` and `npm run docs:dev`, or `npm run docs:build` for GitHub Pages. Set GitHub Pages source to **GitHub Actions** and run the documentation workflow. For a custom domain set `DOCS_BASE=/`; project sites default to `/<repository>/`. The previous static docs remain in [`docs/site`](docs/site/) until migration is complete. The experimental Go server provides liveness and an optional separate local API with opt-in create-only uploads, but is **not a production replacement**; see [migration status](docs/go-migration.md).

## Quick deployment

**Fork owners:** the following buttons point to the upstream repository, not your fork. Do not use them to deploy your fork; select your own repository and branch in the provider dashboard instead.

[![Deploy upstream to Render](https://render.com/images/deploy-to-render-button.svg)](https://render.com/deploy?repo=https%3A%2F%2Fgithub.com%2FMoLik-Labs%2FKova-INC)
[![Deploy upstream Cloudflare edge proxy](https://deploy.workers.cloudflare.com/button)](https://deploy.workers.cloudflare.com/?url=https%3A%2F%2Fgithub.com%2FMoLik-Labs%2FKova-INC%2Ftree%2FBETA%2Fdeploy%2Fcloudflare-worker)

Cloudflare's button requires a public repository, and Render users need access
to the repository. Buttons use `BETA`,
so changes in an unmerged branch are not included. Cloudflare deploys only the
edge proxy; it needs a separate HTTPS origin and `KOVA_ORIGIN` secret. Render
creates a paid persistent disk and generates an initial admin password; review
the plan and costs before confirming and retrieve the password from Render
service environment settings. These links target the upstream repository, not
your fork; for a fork select your repository in the provider dashboard.
An isolated [Cloudflare-native R2 reader and opt-in upload prototype](deploy/cloudflare-native/README.md) is not a complete backend and must not be used for production. A separate [Cloudflare Pages proxy](deploy/cloudflare-pages/README.md) also
requires that origin. Deploying `web` as static files alone cannot provide the
server or data. See the [platform deployment guide](docs/platform-deployment.md) for Railway,
Fly.io, Heroku, persistence, backup, scaling and first-start instructions.

## Build and run

### Requirements

- Linux (the local filesystem fast path uses Linux APIs)
- A C++20 compiler (GCC 11+ or Clang 14+), Make, and development libraries for SQLite, libcurl, OpenSSL, and zlib
- Node.js 22+ for web UI checks and optional Cloudflare Worker tooling; no Node.js is needed at runtime

```bash
# Build optimized, hardened executable
make -j"$(nproc)" release

# Start the server (default: 0.0.0.0:5244, data directory: ./data)
./alist server --data ./data --port 5244

# Show supported commands and build identity
./alist --help
./alist version
```

On first startup, Kova creates the data directory, database, signing secret, and administrator account. The generated administrator password is written to the server log; set `ALIST_ADMIN_PASSWORD` before first startup to choose it in advance. Once the database exists, change or reset the administrator password with:

```bash
./alist admin set 'use-a-strong-password' --data ./data
# Or generate a new random password:
./alist admin random --data ./data
```

The data directory contains the SQLite database, generated configuration/signing secret, logs, and temporary downloads. Keep it private and persistent; back it up before upgrades.

### Docker Compose

```bash
docker compose up -d --build
```

Compose publishes port `5244` by default and persists data at `./data`. The image runs as an unprivileged account by default (`PUID=10001`, `PGID=10001`). Set those values to match your host’s data-directory ownership. For a reverse-proxy deployment, terminate TLS at a trusted proxy and enable proxy-header trust only when that proxy is under your control.

A Render Blueprint is provided in [`render.yaml`](render.yaml); it configures the `/ping` health check and persistent storage under `/opt/alist/data`. The server honors the platform-provided `PORT` environment variable unless `ALIST_HTTP_PORT` is explicitly set.

### Heroku Container Stack

A Docker-based Heroku configuration is included in [`heroku.yml`](heroku.yml):

```bash
heroku create <app-name>
heroku stack:set container -a <app-name>
git push heroku HEAD:main
```

Heroku supplies `PORT`, which Kova reads automatically. **The standard Heroku dyno filesystem is ephemeral.** Without an explicit `ALIST_DATA_DIR`, the container creates a private temporary data directory for its non-root user on each start; the SQLite database, signing key, users, and storage configuration are lost on restart. This is suitable only for demos/testing. Do not rely on a standard dyno filesystem for durable production data. For a straightforward persistent deployment, use Render with the included disk or another container host with a writable persistent volume.

For other container platforms, keep `/opt/alist/data` on persistent storage, expose the port the service injects through `PORT` (or configure `ALIST_HTTP_PORT`), and ensure the volume is writable by the configured `PUID`/`PGID`. For a container started as a non-root UID, set ownership on the volume for that UID; PUID/PGID cannot change it. An explicit `ALIST_DATA_DIR` overrides the data path, and custom directories must already be writable by the runtime user.

### Cloudflare Worker edge proxy

The Worker under [`deploy/cloudflare-worker`](deploy/cloudflare-worker/) is an **edge reverse proxy**, not a standalone Kova server. It requires a separately deployed Kova origin with persistent storage. Set the Worker secret `KOVA_ORIGIN` to that origin's HTTPS URL, then deploy with Wrangler; see the [Worker deployment guide](deploy/cloudflare-worker/README.md). Cloudflare request-size and timeout limits still apply to streamed transfers. Restrict direct origin access where possible, and do not enable `ALIST_TRUST_PROXY` unless callers cannot bypass the trusted proxy.

## Configuration

Settings are stored in `config.json` under the data directory. Environment overrides are applied at startup:

| Variable | Default / behavior |
| --- | --- |
| `ALIST_DATA_DIR` | `data`; ignored when `--data` is supplied |
| `ALIST_ADDR` (`ADDR`) | `0.0.0.0` |
| `ALIST_HTTP_PORT` (`PORT`) | `5244`; CLI `--port` takes precedence |
| `ALIST_SITE_URL` | Empty; set the public HTTPS origin when absolute links are needed |
| `ALIST_ADMIN_PASSWORD` | Random on first initialization; only used if no administrator has been created yet |
| `ALIST_JWT_SECRET` | Uses the persisted random secret unless a value of at least 32 characters is supplied |
| `ALIST_TOKEN_EXPIRES_IN` | `48` hours |
| `ALIST_DEBUG` | `false` |
| `ALIST_ALLOW_PRIVATE_NET` | `false`; enables outbound access to private/reserved networks when set to true |
| `ALIST_TRUST_PROXY` | `false`; trust forwarded client-address headers only behind a trusted proxy |

The file-backed configuration also controls upload size, worker counts, connection limits, rate limits, and TLS verification. Treat the data directory and any injected secrets as sensitive.

## Tests and measurements

A single benchmark run in the development sandbox on 2026-11-16 measured the following; these are host-specific reference numbers, not service-level guarantees:

| Metric | Result |
| --- | ---: |
| Stripped release executable | 1,169,720 bytes |
| Initial / loaded process RSS | 6.86 / 20.78 MiB |
| Sharded cache lookups | 8.97 M ops/s |
| AES-256-GCM encryption | 4,247.4 MiB/s |
| HS256 JWT verification | 218,073/s |
| SQLite FTS5 (10k indexed files) | 10,965 queries/s (91.2 µs/query) |
| HTTP/1.1 keep-alive | 75,229 req/s (20,000/20,000 verified) |
| `sendfile64()` stream | 5,833.6 MiB/s (48.94 Gbps) |

These are local native measurements, not a language-to-language comparison. Kova remains C++20 for now: it already has a streaming I/O path and Linux `sendfile` fast path, while a whole-application Go/Rust rewrite would have to reproduce the API, storage drivers, WebDAV behavior, database, and security fixes without a demonstrated end-to-end gain. Revisit a port only if profiling a representative workload identifies a CPU-bound bottleneck and a comparative prototype meets a measurable goal. The Cloudflare Worker is a separate JavaScript edge proxy; it does not require or imply a server-language port.

Re-run on the target machine to compare against its hardware, kernel, compiler, and libraries:

```bash
# Native API, WebDAV, security, archive, database and concurrency suites
make -j"$(nproc)" test

# Instrumented native suites
make -j"$(nproc)" asan   # ASan, LeakSanitizer, UBSan
make -j"$(nproc)" tsan   # ThreadSanitizer

# Optimized benchmark suite
make -j"$(nproc)" bench

# Bundle integrity and front-end interaction smoke tests
node scripts/build_webui.mjs --check
npm ci --ignore-scripts --no-audit --no-fund
make webui-test
npm run test:worker
(cd deploy/cloudflare-worker && npm ci --ignore-scripts --no-audit --no-fund && npm test && npx wrangler deploy --dry-run)
```

The front-end smoke test uses a DOM implementation to exercise UI state and interactions; it is not a substitute for running the app in a real browser. Benchmark results are host- and load-dependent; use `make bench` on the target machine instead of treating sample numbers as guarantees.

## Security notes

Security controls include bounded request parsing and streaming, local path/symlink confinement, restricted outbound URL validation with DNS pinning and hop-by-hop redirect checks, credential stripping on cross-origin redirects, task cancellation and queue limits, public-share visibility/quota checks, and ZIP bounds/encryption/ratio checks. Embedded UI delivery uses representation-specific ETags, gzip negotiation, and a strict CSP; CSP generation fails closed if the expected bundle shape changes.

Review [`skills/README.md`](skills/README.md) for the reusable design and testing references saved with this repository.
