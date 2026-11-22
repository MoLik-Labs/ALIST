# Deploying the experimental Go preview

**This is not the complete Kova application and is not production-ready.** The
Go preview currently serves a single token-protected local directory, a separate
workspace UI, temporary file shares and a limited WebDAV implementation. It does
not yet implement accounts, cloud storage drivers, persistent shares or the native API. The native
server remains the functional version. Use test data, not a production store;
see [Go migration status](go-migration.md).

All deployment instructions below use `Dockerfile.golang` and a dedicated `/data`
root. They do not migrate the C++ database. The server listens on `0.0.0.0` in
the image and uses `ALIST_HTTP_PORT`, then the platform's `PORT`, then 5244.
The unauthenticated `/ping` endpoint confirms **only process liveness**, not
storage durability, readiness or feature parity. The app does not terminate
TLS: use HTTPS at the platform edge or a trusted reverse proxy. Do not publish a
raw HTTP origin or pass credentials over insecure networks.

## Configuration

Set `KOVA_GO_TOKEN` through your platform's secret manager to a strong random
secret of at least 32 bytes. Anyone with this token has all enabled capabilities;
there is no account isolation. Never commit the token or include it in a URL.
The image uses `/data` as its fixed root; configure the volume at `/data`.

| Variable | Default | Effect |
| --- | --- | --- |
| `KOVA_GO_ALLOW_WRITE` | `false` | Allow create-only file uploads and directories; no overwrite |
| `KOVA_GO_ALLOW_DELETE` | `false` | Delete files and empty directories; irreversible |
| `KOVA_GO_ALLOW_MOVE` | `false` | Move/rename without overwriting; Linux amd64/arm64 only |
| `KOVA_GO_ENABLE_WEBDAV` | `false` | Enable the limited `/dav/` endpoint |
| `KOVA_GO_ENABLE_SHARES` | `false` | Enable experimental temporary file shares |

Use exactly `true` or `false` in lowercase; unset or empty means false. Invalid
values prevent startup, and CLI flags override the corresponding environment
default. WebDAV write, delete and move operations require their own permission
variables too. The upload limit is 64 MiB; WebDAV is incomplete and has not
been validated against external clients. See the migration document for more
restrictions and the reverse-proxy limitation on WebDAV MOVE.

The image requests UID/GID 10001. A mounted directory can mask the ownership
set at image build time: verify the **actual runtime user** has traversal/read
access and, if write operations are enabled, write access. Do not make data
world-writable to work around a permissions issue. Untrusted local OS users
must not be able to mutate the root. Mounted data requires separate backups;
never treat an image rebuild, deployment rollback or container restart as a
backup.

## Docker Compose / self-hosted

1. Create a dedicated test `go-data` directory; configure its owner and
   permissions for UID/GID 10001 without exposing it to untrusted local users.
2. Supply `KOVA_GO_TOKEN` securely to Compose; do not commit a `.env` file
   containing the secret. Optional variables above default to false.
3. Run `docker compose -f docker-compose.go.yml up --build` and visit
   `http://127.0.0.1:5244/` on the host. The default published port binds to
   loopback only. Use a correctly configured HTTPS reverse proxy for remote use.

Use one instance per local root until shared storage and concurrency semantics
have been designed and tested. Set `PORT=5244` inside the container if a host
setup happens to supply a different port.

## Render

Create a Blueprint from the repository and set **Blueprint Path** to
`render-go.yaml` (the existing `render.yaml` is for the native application).
The preview Blueprint requests a paid service and a 1 GB disk at `/data`;
confirm pricing and region before creating it. Enter `KOVA_GO_TOKEN` in the
Render dashboard. Set optional operation variables only if needed. Check the
mounted disk's actual permissions for UID 10001 before enabling writes. `PORT`
is injected by Render and `/ping` is the liveness check. A persistent disk has
platform restrictions on scaling and deployments; read Render's current disk
documentation and keep backups. Rollback does not roll back disk contents.

## Railway

Create a **separate** service from the repository, select the Dockerfile builder,
and configure the Dockerfile path to `Dockerfile.golang` in service settings.
Attach a Railway volume mounted at `/data`, set a random `KOVA_GO_TOKEN`, and
optionally configure the permissions. Set health check path `/ping` and
make sure the HTTP service binds the injected `PORT`. Confirm volume ownership
for the runtime user and use one replica. Do not rely on ephemeral container
filesystem data. Configure these settings in the dashboard or Railway's current
infrastructure-as-code tooling; older `railway.json`/`railway.toml` Config as
Code is deprecated for new services. This repository does not change the
existing default `Dockerfile` or claim to auto-configure Railway.

## Heroku — ephemeral demonstration only

Heroku's dyno filesystem is ephemeral and does not support volume mounting.
The current Go implementation has no connected remote storage driver, so it
**cannot provide durable file storage on Heroku**. Do not enable writes for
anything you need to keep; dyno restarts, deploys and additional dynos do not
share files. Use Render/Railway with a verified persistent volume or self-hosting
for storage experiments instead.

To try the read-only empty preview, deploy `Dockerfile.golang` via Heroku's
container registry and release the `web` process; configure `KOVA_GO_TOKEN`
securely as a config var. The existing `heroku.yml` deliberately builds the
native `Dockerfile`, not the Go preview; do not accidentally deploy it when
following this section. Heroku injects `PORT` and may run as a different UID;
verify that `/data` can be read in the actual dyno. This preview has not been
deployed to Heroku.

## Validation status and references

The local Go subprocess smoke test (`make go-build` followed by
`python3 tests/go_deployment_smoke.py`) checks `PORT`, token auth, disabled/enabled
writes, WebDAV and graceful SIGTERM. It does not execute a Docker image or test
volume permissions, proxy TLS, platform routing or a real WebDAV client. No
Docker build or Render, Railway or Heroku deployment was performed here.

- [Render Blueprints](https://render.com/docs/infrastructure-as-code)
- [Render persistent disks](https://render.com/docs/disks)
- [Railway volumes](https://docs.railway.com/volumes)
- [Railway Config as Code deprecation](https://docs.railway.com/config-as-code)
- [Heroku container runtime](https://devcenter.heroku.com/articles/container-registry-and-runtime)
