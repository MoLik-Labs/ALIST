# Go migration status

The Go implementation is **in progress and is not a replacement for Kova**.
Its experimental server exposes `/ping` and, when explicitly configured, a
separate token-protected local-file API with optional create-only uploads. Other
endpoints return 503 except an optional limited WebDAV implementation. It includes a
separate embedded local-workspace UI, but cannot replace the native
deployment yet. The C++ implementation remains authoritative until its remaining
functionality is ported and verified. Do not use the Go binary for production or
point it at an existing database until compatibility and migration have been tested.

To build the experimental binary (requires Go 1.22+; CGO and a C compiler are
required for the separate SQLite compatibility tests, not this binary):

```bash
go build -tags sqlite_fts5 -o build/alist-go ./cmd/alist-go
./build/alist-go version
./build/alist-go server --experimental --addr 127.0.0.1 --port 5244
```

The server binds to loopback by default. The explicit opt-in is required because
`/ping` is only liveness, not readiness. To exercise the separate experimental
local API, set a random secret of at least 32 bytes in `KOVA_GO_TOKEN` and
set `KOVA_GO_ROOT` (or pass `--root`) to a dedicated test directory. Use HTTPS
or loopback; never send the token over an untrusted plaintext connection. Then:

```bash
curl -H "Authorization: Bearer $KOVA_GO_TOKEN" 'http://127.0.0.1:5244/api/local/list?path=/'
curl -H "Authorization: Bearer $KOVA_GO_TOKEN" 'http://127.0.0.1:5244/api/local/file?path=/example.txt'
```

Open the server's root URL to use the embedded Go workspace. Its token is kept
in memory only; logging out clears the browser copy but does not revoke the
server token. Browser downloads are limited to 128 MiB to bound memory; use the
API for larger files. The list response reports whether create and delete are
enabled, so the UI hides unavailable controls. Server-side permission checks
remain authoritative. This is a new UI, not the native application's UI.

These are new experimental endpoints, **not AList-compatible**. The Linux-only
backend uses descriptor-relative `openat` and rejects symlinks and traversal;
reopening a checked file requires procfs at `/proc/self/fd`. It cannot prevent
access through hard links already present inside the root, or other users with
OS access mutating files. Use a dedicated directory and account. Uploads are
**disabled by default**. `--allow-write` enables create-only PUT to
`/api/local/file?path=/new-file`, with a 64 MiB limit, eight concurrent uploads,
and a five-minute read deadline. Uploads use a private temporary file followed
by an atomic hard-link publication; existing paths are never overwritten. The
filesystem must support hard links. Interrupted uploads are cleaned up normally;
a process crash may leave hidden `.kova-upload-*` files, and crash durability
is not guaranteed. `--allow-delete` separately enables deletion of regular files
and empty directories, including through the UI; it is disabled by default. It
never recursively deletes a directory, and symlinks and the root cannot be
removed. Deletions are immediate and have no recycle bin. Keep the storage root
inaccessible to untrusted OS users: a local actor able to rename entries may
race a deletion. `--allow-move` separately enables non-overwriting moves through
`POST /api/local/move?path=/old&destination=/new`; the workspace UI exposes
same-directory rename. It supports files and directories, not the root, symlinks,
missing parents or overwrites. Moves are atomic only within a filesystem, using
Linux `renameat2(RENAME_NOREPLACE)` on amd64/arm64; other architectures,
unsupported kernels/filesystems and cross-filesystem moves fail rather than
falling back to an unsafe overwrite. The capability response detects architecture,
not filesystem support. As with deletion, untrusted local users must not be able
to mutate the storage root during a move. With `--enable-webdav` it also enables
WebDAV MOVE with the same restrictions. The workspace UI does not implement
accounts, metadata policies, a persistent share list or cloud drivers.

Add `--enable-shares` (or `KOVA_GO_ENABLE_SHARES=true`) to opt into temporary,
read-only, single-file links. Authenticated `POST /api/local/shares` accepts JSON
`{"path":"/file.txt","ttl_seconds":3600}` and returns a relative `/s/...` URL
and expiry timestamp. TTL must be 60 seconds to seven days. The bearer token
can revoke one link using `DELETE /api/local/shares/<id>`. Anyone holding a link
can download the file until expiry or revocation; links are credentials. Use
HTTPS, prevent access logs, proxies, analytics and browser history from leaking
URLs, and share only with intended recipients. There is no password, access
count, user authorization or persistent storage for shares. All shares
are lost on process restart and are not shared among replicas. Authenticated
`GET /api/local/shares` lists unexpired links, including their bearer URLs; the
list is a point-in-time snapshot and can race a concurrent revoke. The UI can create one-hour links and list or revoke active links in this process;
closing the panel or logging out does not revoke them. A request that reaches
the server may create a share even if the user leaves the page before receiving
the response; the administrator can find it in the list. At most 1,000
unexpired links exist per process; expiry cleanup is lazy. Revocation does not
interrupt a download already in progress. Links refer to live files, not
snapshots: edits to the same inode change what recipients see. The server
checks file identity on access, but inode reuse after deletion can defeat this
check; do not rely on it to isolate changing or untrusted local data.

Add `--enable-webdav` to expose an **experimental WebDAV subset** at `/dav/`.
Use HTTP Basic username `kova` and the same `KOVA_GO_TOKEN` as password, or use
Bearer authentication. Basic credentials must only travel over HTTPS or loopback.
It supports OPTIONS, PROPFIND depth 0/1 and regular-file GET/HEAD with Range.
If `--allow-write` is also set, MKCOL and create-only PUT are enabled: existing
files are never overwritten, missing parents are not created, and PUT shares the
64 MiB limit. Unsupported conditional writes are rejected. Depth infinity,
PROPPATCH, COPY and locks are not implemented. `--allow-move` enables MOVE to a
new path only: an existing destination returns 412 for `Overwrite: F` and 403
otherwise; overwrite is never performed. Destination must be root-relative or
an absolute URL matching the request host and the server-side HTTP/TLS scheme;
forwarded headers are not trusted. Consequently, HTTPS-terminating proxies may
need clients to use root-relative destinations until a trusted proxy policy is
implemented. `--allow-delete` enables DELETE for files and empty collections
only, never recursive collection deletion.
PROPFIND inspects at most 1,000 raw directory entries (including hidden or
unsupported entries), limits generated XML to 4 MiB, and allows eight concurrent
requests; a larger listing or response is rejected rather than truncated. The
local API and WebDAV have independent eight-upload limits. WebDAV client
interoperability has not yet been validated.

`Dockerfile.golang`, `docker-compose.go.yml` and `render-go.yaml` provide **separate
experimental** Docker/Render configurations. Set `KOVA_GO_TOKEN` to a random
secret of at least 32 bytes; provide a dedicated persistent `/data` volume
accessible to UID/GID 10001. Compose binds to localhost by default; put HTTPS in
front before exposing it. Render injects `PORT`, terminates TLS and uses the
specified persistent disk; verify that the disk is accessible by the container
user. The default image does not enable writes. These configurations have not
been built with Docker or deployed to Render in this environment.

## Implemented

- `internal/vpath`: normalization, joining, scope checks and filename validation,
  matching the current native virtual-path rules, including user-relative path
  resolution and its legacy invalid-base fallback to `/`. These checks do not
  authorize access, resolve symlinks or confine local filesystem access.
- `internal/cache`: bounded 64-shard LRU/TTL cache, path-prefix invalidation,
  per-shard counters and concurrent tests. Stored mutable values still require
  caller synchronization; cross-shard operations are not globally atomic.
- `internal/mounts`: canonical-path mount routing, virtual children and replica
  round-robin using atomically replaced immutable snapshots. It is only the
  routing table: authorization, storage drivers and resource lifetime management
  are not implemented here. Mounted values must remain valid for in-flight work.
- `internal/s3store`: an experimental S3-compatible single-object streaming GET
  client using SigV4 query authentication. It supports session credentials,
  HTTPS by default, and guarded outbound connections without redirects; LAN and
  plain HTTP require separate opt-ins. Its signing matches the published AWS
  example, and private-address rejection and streaming are tested locally. It
  is **not yet connected** to the server, does not list, write, delete, or handle
  multipart uploads, and has not been tested against real S3, R2, or MinIO.
  Presigned URLs are credentials and are not exposed by the public API; callers
  must close streams and bound their reads. The client rejects some valid S3 keys
  (including dot segments and repeated slashes) rather than normalizing them.
- `internal/outbound`: an experimental HTTP(S) client that checks every DNS
  answer, dials a checked IP, verifies TLS against the URL hostname, rechecks
  redirects and strips cross-origin headers. It rejects private and reserved
  destinations by default, and does not use environment proxies. Its more
  restrictive address policy is not identical to native Kova; FTP, FTPS and
  SFTP are not implemented. It is not yet connected to storage drivers.
- `internal/nativedb`: experimental access to a subset of an **existing**
  native SQLite schema, including user and storage reads, encrypted credentials,
  settings, atomic TOTP replay-step updates, atomic share quota reservations,
  and FTS5 index/search operations. `make go-db-compat-test` tests against an
  actual native-created database and reopens Go-written data in C++. The package
  cannot initialize or migrate a database and does not implement authorization,
  share expiry/password checks, or all remaining CRUD operations. Search callers
  must enforce access and hide rules on every result. It requires CGO and a C
  compiler. Test only on an offline copy; concurrent use with C++ may leave
  native caches stale. The SQLite driver's default build does not enable FTS5:
  use `go test -tags sqlite_fts5 ./...` (and the same tag when building any future
  Go server) to enable it. Invalid stored paths and invalid index names are
  rejected rather than normalized permissively.
- `internal/cryptocompat`: native-compatible vault encryption/decryption,
  signed URLs, HS256 JWT, PBKDF2-SHA256 password hashes and six-digit TOTP.
  Vault failures are returned as errors rather than silently falling back to
  plaintext or treating authentication failures as empty values. JWT verification
  alone is not authentication: user status, password timestamp and revocation
  still need checking. TOTP replay protection must be enforced atomically by the
  caller. Run `make go-compat-test` for live cross-language tests against C++
  vault, JWT and password code, plus signed URL and TOTP compatibility. Go
  verification deliberately bounds input sizes and rejects some malformed or
  non-canonical encodings accepted by the native decoder. PBKDF2 is vendored
  from `golang.org/x/crypto` v0.31.0 with its license and upstream test vectors.
- `cmd/alist-go`, `internal/goserver` and `internal/localfs`: experimental
  CLI and HTTP server with a required opt-in, graceful shutdown, liveness,
  an embedded local-workspace UI and token-protected local API with explicitly
  opt-in create-only uploads and directory creation. An opt-in WebDAV subset
  supports reads and, when enabled, MKCOL, create-only PUT and non-overwriting
  MOVE. File and empty-directory deletion requires a separate opt-in; unsupported
  routes and methods fail closed.
  Database, user authentication, VFS and cloud drivers are not yet connected.
- Unit and fuzz tests for path and vault semantics. Run `go test ./...` and
  `go test -race -tags sqlite_fts5 ./...` using Go 1.22 or later.

## Required before replacement

1. Capture compatibility tests against the C++ server, including API responses,
   WebDAV, URL signatures, encrypted files, SQLite schema, credentials and data.
2. Port configuration, database and migration, crypto/auth/security, HTTP,
   streaming, VFS and all drivers, archive, tasks, API, WebDAV and UI embedding.
3. Test upgrades on copies of real datasets, concurrency/race, failure recovery,
   security and performance under representative workloads. Compare against the
   current implementation; a language change alone does not guarantee speed.
4. Only after parity, update Docker, CI and deployment and choose a cutover and
   rollback strategy. Retain the native server until those checks pass.

For separate experimental Docker, Render, Railway and Heroku setup and their
storage limitations, see [Go deployment](go-deployment.md).

## Cloudflare

The existing Worker is an optional reverse proxy to a separately hosted origin,
not a standalone Go server. A Go origin can later sit behind it, subject to
end-to-end testing of streaming, WebDAV and forwarded-header trust. Running the
entire server in Workers would require a separate architecture for persistent
state, filesystem and networking; it is not assumed by this migration.
