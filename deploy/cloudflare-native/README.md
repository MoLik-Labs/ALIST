# Experimental Cloudflare-native backend

This is an isolated proof of concept, **not a Kova replacement** and not
production-ready. It runs in Workers without a separate Kova origin and reads an
R2 bucket through a binding. It has a liveness endpoint (`GET /ping`), bearer
protected `GET /v1/objects` (100 objects per page, optional `prefix` and opaque
`cursor`) and streaming `GET /v1/object?key=...`. An optional create-only `PUT /v1/object?key=...` accepts a declared length up to 8 MiB and is disabled by default. It is not connected to the UI.

To experiment, create a separate Worker project and isolated R2 bucket; bind the
bucket as `R2` and set `KOVA_NATIVE_TOKEN` to a unique random secret of at least
32 characters through Cloudflare's runtime secret settings. Use HTTPS. Do not
bind production data. There is deliberately no one-click button: the prototype
has no user accounts, D1 metadata, large/multipart uploads, deletion, permission scopes, rate
limiting, session revocation, WebDAV, legacy API compatibility, backup workflow,
or real-provider/browser testing. Possession of the single token grants read
access to the **entire bucket**, including create access if writes are enabled. Do not expose this on a public service or use
it to store sensitive data. Hash comparison and error redaction are not a
substitute for a security review. Cloudflare platform and R2 limits apply.

To test writes locally only, add `--var KOVA_NATIVE_WRITES:enabled` to the
Wrangler command below and run `TEST_LOCAL_WRITES=1 node
tests/cloudflare_native_runtime_smoke.mjs`. The test leaves a random test object
in the emulated bucket. Writes require `Content-Length`, are buffered in memory,
are limited to 8 MiB and use an R2 conditional request to avoid overwriting an
existing key. That behavior was tested locally, not with real R2. Do not enable
writes or use production data until there is a security review and real-cloud
verification.

## Local runtime test (no Cloudflare account)

Install the pinned Wrangler dependencies with `npm ci --ignore-scripts` in
`deploy/cloudflare-worker`. Then from the repository root run:

```sh
deploy/cloudflare-worker/node_modules/.bin/wrangler dev \
  --config deploy/cloudflare-native/wrangler.local.toml --local \
  --ip 127.0.0.1 --port 8787 \
  --var KOVA_NATIVE_TOKEN:local-test-secret-at-least-32-characters
```

In another terminal, run `node tests/cloudflare_native_runtime_smoke.mjs`
or check `curl http://127.0.0.1:8787/ping`, then:

```sh
curl -H 'Authorization: Bearer local-test-secret-at-least-32-characters' \
  'http://127.0.0.1:8787/v1/objects'
```

This is an emulated, empty R2 bucket and a public example token for local use
only. Do not deploy this configuration, use this token remotely, or replace
it with a real secret on a command line. Emulation does not verify Cloudflare
account bindings, resource limits or real R2 behavior. Stop the server when done.

## Experimental Google Drive connector

The isolated Worker also has a **single-account, read-only prototype**:
`GET /v1/google-drive/files?parent=root` lists a folder (optional opaque
`cursor`; the response has `nextPageToken`) and
`GET /v1/google-drive/file?id=...` streams a binary file. These routes use the
same bearer token as the R2 API and do not require the R2 binding. Anyone with
that token can access **all files authorized to the connected Google account**,
not just one folder. Native Google Docs, Sheets and Slides require export and
are not supported. Upload, search, range/resume, shortcuts, OAuth consent UI,
per-user credentials, permissions, credential revocation, rate limiting and
multi-drive management are not implemented. There is no Kova UI integration.

For a future isolated experiment, Google Drive API must be enabled, an OAuth
client and an offline refresh token obtained securely with a suitable read-only
scope, and `GOOGLE_CLIENT_ID`, `GOOGLE_CLIENT_SECRET` and
`GOOGLE_REFRESH_TOKEN` set as Worker runtime secrets, alongside a strong
`KOVA_NATIVE_TOKEN`. **Do not paste credentials in commands, issues or chat.**
Use a disposable Google account and files only. Google OAuth verification,
testing-mode restrictions, token expiry/revocation and quotas apply; a refresh
token is not guaranteed to remain valid. Consult Google's
[web server OAuth guide](https://developers.google.com/identity/protocols/oauth2/web-server)
and [Drive scope guide](https://developers.google.com/workspace/drive/api/guides/api-specific-auth).
This code refreshes on every request; it does not cache tokens or retry quota
errors. It fixes upstream hosts and rejects redirects, strips upstream headers,
limits JSON responses, and never returns credentials in errors. Downloads have a
30-second deadline; large files and redirects may fail. **Mocks only:** this has
not been exercised against a real Google account or deployed Worker. Do not use
it for production or sensitive data. Revoke credentials after experimentation.
