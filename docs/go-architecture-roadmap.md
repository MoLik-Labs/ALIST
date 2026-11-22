# Go replacement roadmap

## Decisions

The Go version is a new ecosystem with fresh data. Preserve important existing
API and WebDAV behavior where practical, but prioritize correctness, measured
performance and maintainability rather than assuming a language switch is faster.
Support all existing storage categories incrementally, starting with an
S3-compatible foundation. Build professional multi-user administration and
least-privilege permissions. Docker/self-hosting, Render, Railway and Heroku
must be viable; on Heroku, durable state must be stored outside the dyno.

## Persistence model

A durable deployment needs both **file data** and **application metadata** to
survive restarts. Object storage alone does not persist user accounts, mounts,
permissions or shares. The intended cloud architecture is external S3-compatible
object storage plus an external transactional database such as managed
PostgreSQL, with encryption keys stored in the platform secret manager. The
self-hosted profile may use SQLite on a persistent volume, if migration,
concurrency, backups and recovery are implemented and tested. None of those
metadata backends is connected to the experimental server yet. Heroku's
ephemeral filesystem cannot provide durable local storage. A reverse proxy or
Cloudflare Worker is not a substitute for either storage or database.

## Implementation sequence

1. S3-compatible driver: signed reads, streaming, listing and pagination,
   uploads/multipart, deletion, failure/cancellation handling and integration
   tests against a real compatible service. Start with a narrow read-only client;
   do not expose it to users until authorization and configuration are wired.
2. Storage abstraction, VFS mounts and configuration, guarded outbound
   connections, safe credential storage and administrator setup.
3. Durable database and schema migrations for users, mounts and shares; design
   PostgreSQL and local SQLite lifecycles explicitly. Add transactions,
   backup/recovery tests and upgrade tests.
4. Accounts, roles, path-scoped permissions, session revocation, 2FA and secure
   administration. Integrate authentication before exposing real drivers.
5. Durable sharing, background tasks, search, archive handling, WebDAV and the
   most important legacy API contracts; implement and test other storage drivers
   individually, including OAuth lifecycles and provider-specific semantics.
6. Real provider, browser, container and platform tests; representative
   performance comparisons, security review and a documented rollback plan.
   Keep the C++ server until required functions and compatibility have been
   verified, not just reimplemented in isolation.

These are architectural goals, not claims of implemented functionality. The
current state and exact limitations are in [go-migration.md](go-migration.md).
