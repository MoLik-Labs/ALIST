# Security and recovery

1. Use HTTPS. Protect the administrator account with a strong unique password and 2FA. Never put passwords or API tokens in the repository, screenshots or build logs.
2. Keep guest access disabled unless intentionally configured. Grant users only the paths and actions they need; review public shares and expiry.
3. Restrict access to the server and data directory. Do not enable private-network outbound requests unless required and trusted. Never trust forwarded headers if clients can bypass your proxy.
4. Back up the database consistently, including SQLite WAL state, together with configuration and signing keys. Store backups securely away from the host. Restore to an isolated environment and verify login and data before relying on a backup.
5. Keep staging, preview and production credentials and storage separate. Monitor deployment logs and update deliberately. Cloudflare request limits and provider outages can affect uploads; test actual clients and file sizes.

`/ping` is a liveness check, not proof of working storage or backup. Check real file operations and recovery. Security controls reduce risk; no deployment can be guaranteed invulnerable.
