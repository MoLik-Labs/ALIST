# Cloudflare product direction (design, not implemented)

## Agreed goals

Kova connects cloud drives. Prioritize an independent Cloudflare backend, with
no Kova origin server, and the existing Kova frontend/API feature set as the
long-term target. Support multiple users and all existing drivers where feasible.
Use free tiers; never require a paid plan as a condition of the free deployment.
Deliver incrementally, label unfinished work, and explicitly defer unsupported
capabilities rather than silently faking them. Quotas, availability and terms can
change; free services can stop serving requests after limits are reached.

The bilingual GitHub Pages documentation is a separate static website, not
Google Docs and not the backend. Cloudflare runtime and provider integrations
must be verified in real accounts before being described as production-ready.

## Privacy and sharing

- Default new uploads, items and connections to private. Require explicit action
  to make an item public or create a link; make scope and effect visible.
- An administrator grants roles. Every sensitive operation must be authorized on
  the backend, not only hidden in the frontend. Define narrowly scoped actions
  (read, write, delete, manage shares, manage users and manage connections) and
  test least privilege, revocation and cross-user isolation.
- Access to private content is determined by explicit role and resource grants,
  not automatically by the role name. Administrative capabilities must be
  visible and auditable. A Cloudflare account owner who can alter Worker code,
  bindings or secrets is a deployment operator and cannot be made technically
  blind to plaintext solely by application-level roles. Do not claim end-to-end
  encryption or privacy from such an operator without a separate design.
- Support recipient-restricted links and unlisted bearer links. Require an
  explicit choice; use recipient-restricted links as the safe default. Bearer
  links grant access to whoever has the link and can be forwarded. Plan expiry,
  revocation, optional password and resource/action scope; store only a hash of
  a high-entropy link token and avoid logging tokens. Verify authorization at
  each request, including downloads and redirects.
- Kova access rules govern access **through Kova only**. Existing public links,
  direct cloud-provider permissions, and other clients may still expose an
  object. A Kova private setting must not claim to change provider ACLs.

## Delivery gates

1. Inventory current drivers and actual operations, API routes and permissions;
   document runtime restrictions and free-tier limits for each. Local filesystem,
   FTP/SFTP and other network-dependent drivers require particular investigation
   and must not be advertised as Worker-compatible without verification.
2. Design and implement durable D1 metadata, credential protection, bootstrap,
   sessions, roles and authorization before granting multi-user drive access.
   Isolate preview resources and keys from production. Plan recovery and backup.
3. Implement one real provider vertically (connection, refresh, UI, operations,
   permissions, disconnection), test it with a disposable real account, then
   add the other drivers and Kova features in priority order. Unsupported
   operations fail explicitly. Never request credentials in chat or commit them.
4. Add per-provider automated tests, adversarial authorization tests and real
   cloud smoke tests. Test large transfer and request limits; document limitations
   before enabling uploads, shares or public deployment. Monitor free-tier usage.
5. Only mark a component stable or merge a release when its stated acceptance
   criteria are met; prototypes and mocked tests do not prove parity or security.

## Current implementation

The experimental Worker R2 API and single-account Google Drive connector use
one bearer token; they have no accounts, per-user permissions or Kova frontend
integration. They do not implement the above model and must not be deployed for
production or attached to sensitive data. See `deploy/cloudflare-native/README.md`.
