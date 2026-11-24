# Security and recovery

1. Use HTTPS. Protect the administrator account with a strong unique password and 2FA. Never put passwords or API tokens in the repository, screenshots or build logs.
2. Keep guest access disabled unless intentionally configured. Grant users only the paths and actions they need; review public shares and expiry.
3. Restrict access to the server and data directory. Do not enable private-network outbound requests unless required and trusted. Never trust forwarded headers if clients can bypass your proxy.
4. Back up the database consistently, including SQLite WAL state, together with configuration and signing keys. Store backups securely away from the host. Restore to an isolated environment and verify login and data before relying on a backup.
5. Keep staging, preview and production credentials and storage separate. Monitor deployment logs and update deliberately. Cloudflare request limits and provider outages can affect uploads; test actual clients and file sizes.

`/ping` is a liveness check, not proof of working storage or backup. Check real file operations and recovery. Security controls reduce risk; no deployment can be guaranteed invulnerable.

### Local uploads and S3 copies

Local uploads with a declared size are rejected if the stream ends early or exceeds that size; the existing target is retained on these failures. File data is synced before the temporary file is renamed, but this is not a guarantee against power loss and does not replace backups. S3 server-side copy requires a valid `CopyObjectResult` with an ETag before reporting success; providers that do not return that standard result may be rejected. These checks do not verify the destination bytes or make a cross-storage move atomic. Test with disposable data on your provider.

S3 listings fetch continuation pages up to 100,000 entries or 1,000 pages; invalid pagination or a later-page failure causes the listing to fail rather than silently return a partial result. Copying from an HTTP source requires a complete HTTP 200 response; redirects and error pages are rejected. These checks have been tested against local mock servers, not real S3-compatible providers.

S3 object lookup now distinguishes an existing file, a folder marker and a
prefix with children from an absent path. A denied HEAD or failed listing is
reported as an error rather than silently treated as an existing folder. This
has been exercised against a local mock; compatibility with each provider still
requires isolated testing.

## Experimental read-only MCP endpoint

The native server has an **opt-in, experimental** stateless MCP JSON-RPC endpoint at `POST /api/mcp`. Set `ALIST_ENABLE_MCP=true` and restart to enable it. This environment variable is not persisted in `config.json`. Use HTTPS via a trusted reverse proxy and protect your login token as a password; avoid admin or system tokens. Create a dedicated user with a restricted base path and grant the MCP access permission only on the required role scopes. Authenticate with `Authorization: Bearer <login-token>`; guests cannot use MCP.

Supported protocol version: `2025-03-26`. The endpoint implements `initialize`, `ping`, `tools/list`, and `tools/call` for metadata-only `fs_list` and `fs_get`. Paths are absolute **within the user's base path**. `fs_list` returns up to 50 entries per page, using the optional `page` argument. Entries also require MCP permission and normal read visibility. Password-protected metadata paths, including inherited passwords even when a nearer metadata record exists, are excluded for administrators and roles with password bypass; no password can be supplied through MCP. No downloads, raw URLs, write tools, stdio, SSE, sessions, OAuth, or upstream tool parity are implemented. Directory loading and filtering still depend on the underlying driver and can process more than one page internally.

Requests with an `Origin` header are rejected, including same-origin browser clients; no CORS access is granted. Non-browser clients must omit `Origin`. Request bodies are limited to 64 KiB at the endpoint (the HTTP server may buffer a request before this check). GET returns 405; server push is unavailable. Client interoperability has not been verified against external MCP clients. Disable the variable if the endpoint is not needed.
