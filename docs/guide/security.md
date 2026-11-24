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
