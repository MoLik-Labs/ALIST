# Deploying Kova on cloud platforms

These instructions deploy the **native server**, not the experimental Go
server. The native server stores its SQLite database, signing key, users and
mount configuration in `/opt/alist/data`. Remote file drivers do not make this
metadata durable. Run **one instance** against one persistent disk; independent
SQLite databases on multiple replicas will diverge. Back up the database
consistently (including WAL state), signing key and configuration off-platform;
test restoration. A volume is not a backup or high availability. Platform plans,
volumes, network traffic and builds may incur charges.

Set a strong `ALIST_ADMIN_PASSWORD` as a platform secret **before first boot**,
or find the generated initial password in protected service logs. A supplied
password is not logged; protect any bootstrap logs containing generated passwords. The variable
only initializes a new database; changing it later does not reset the account.
Protect logs and secrets. Keep `/opt/alist/data` private and writable by the
container's configured PUID/PGID. The image honors `ALIST_DATA_DIR` as an
explicit directory override; when running as root, custom directories must
already be writable by PUID/PGID and are not recursively chowned. With an
orchestrator-provided non-root UID, PUID/PGID cannot change the process identity
and the mounted directory must already be writable by that UID. `/ping` checks process liveness, not storage
readiness or end-to-end access. Verify login, file access, WebDAV if used, and
restart/restore after deployment.

## Render

Use the Deploy to Render button in the root README or create a Blueprint from
your accessible fork. The root `render.yaml` builds the native Dockerfile,
sets a `/ping` health check and mounts a 1 GB disk at `/opt/alist/data`. It uses
a paid `starter` plan in Singapore; review pricing and region. Set the initial
administrator password is generated automatically by Render; retrieve it from
the service environment in the Render dashboard and keep it private. The initial
password is not written to application logs. The button uses
the repository's default branch and auto-deploy is off; update deliberately
from your own fork after reviewing changes. Confirm the disk is mounted and
data survives a restart. To deploy your fork rather than the original, create a
Blueprint in the Render dashboard and select your fork and branch: the README
button explicitly targets the upstream repository. Render disk services have platform-specific scaling and
deployment limitations.

## Railway

Create a project from an accessible repository/fork in the Railway dashboard.
Select the root `Dockerfile` as the build method, attach a persistent volume at
`/opt/alist/data` **before first startup**, set `ALIST_ADMIN_PASSWORD` as a
secret, set the health check to `/ping`, deploy one instance, and generate a
public domain. `PORT` is supplied by Railway and read by Kova. Confirm that the
volume is writable and survives a redeploy. A Railway one-click template has not
been published or tested; do not use a template without checking that it actually
creates the volume. Avoid deprecated `railway.json`/`railway.toml` for new services.

## Fly.io

The root `fly.toml` is a single-machine starting point with a mounted Fly Volume,
HTTPS and `/ping`. Install and authenticate `flyctl`, then:

```sh
fly launch --no-deploy --ha=false
# Select a unique app name and region, and accept the existing configuration.
# Set ALIST_ADMIN_PASSWORD as a Fly secret using the Fly dashboard or CLI.
fly deploy --ha=false --volume-initial-size 1
fly volumes list
fly status
fly apps open
```

Verify that exactly one machine has the `kova_data` volume mounted at
`/opt/alist/data` before using it. Volume creation, storage size and charges
are subject to Fly's current behavior; if automatic volume creation is not
available, create a volume in the same region before deploying. Volumes are
local to a machine and are not replicated. Do not scale this SQLite deployment
out or clone the machine as an HA solution. `immediate` deployment can cause
downtime. The configuration has not yet been deployed to Fly for verification.

## Heroku

The root `heroku.yml` builds the native Docker image; use Heroku Container
Stack as shown in the README. Heroku provides `PORT` and runs the container
with an arbitrary non-root user. If `ALIST_DATA_DIR` is not set, the entrypoint
creates a private temporary directory and prints a data-loss warning; every
process start creates a fresh directory. This behavior is for demos only. **Standard dynos have an
ephemeral filesystem** and do not provide the persistent mounted volume this
server requires. Restarts or deploys can lose accounts, keys and mounts. Use
this only for disposable demos; an S3 mount alone does not solve metadata
persistence. There is no supported durable Heroku implementation in this repo
until a durable external metadata database is implemented and tested.

## Cloudflare Workers and Pages

The Cloudflare button deploys the Worker in `deploy/cloudflare-worker`, not the
native server or database. First deploy an origin with persistent storage, then
supply the HTTPS `KOVA_ORIGIN` secret. See the [Worker guide](../deploy/cloudflare-worker/README.md)
for routing and origin security. For a fork, connect your repository in Workers
Builds, select its deployment branch, and set root directory to
`deploy/cloudflare-worker`; configure `KOVA_ORIGIN` as a runtime secret (build
secrets are not runtime bindings). The README button targets the upstream repo,
not the fork. Cloudflare's deploy button supports Workers,
not Pages. A Pages Functions proxy is also provided in
[deploy/cloudflare-pages](../deploy/cloudflare-pages/README.md); it requires a
separate persistent origin and is not a standalone server. The `web` directory
by itself contains only the UI and cannot provide APIs or data. The proxy does
not cache private workspace responses, and Cloudflare upload limits and origin
latency still apply. Restrict origin access before trusting forwarded headers.

## Verification status

Configuration and proxy tests run locally/CI; cloud account provisioning,
billing, button flows, volume ownership and real provider uploads are **not**
validated by those tests. Validate on your own account before relying on a
particular provider. There are no guaranteed speed or uptime figures: benchmark
real workloads in the target region and monitor CPU, disk, network and latency.
