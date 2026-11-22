# Experimental Cloudflare-native backend

This is an isolated proof of concept, **not a Kova replacement** and not
production-ready. It runs in Workers without a separate Kova origin and reads an
R2 bucket through a binding. It has a liveness endpoint (`GET /ping`), bearer
protected `GET /v1/objects` (100 objects per page, optional `prefix` and opaque
`cursor`) and streaming `GET /v1/object?key=...`. It is not connected to the UI.

To experiment, create a separate Worker project and isolated R2 bucket; bind the
bucket as `R2` and set `KOVA_NATIVE_TOKEN` to a unique random secret of at least
32 characters through Cloudflare's runtime secret settings. Use HTTPS. Do not
bind production data. There is deliberately no one-click button: the prototype
has no user accounts, D1 metadata, uploads, deletion, permission scopes, rate
limiting, session revocation, WebDAV, legacy API compatibility, backup workflow,
or real-provider/browser testing. Possession of the single token grants read
access to the **entire bucket**. Do not expose this on a public service or use
it to store sensitive data. Hash comparison and error redaction are not a
substitute for a security review. Cloudflare platform and R2 limits apply.

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
