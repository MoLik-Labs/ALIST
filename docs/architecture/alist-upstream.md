# AList upstream compatibility: starting point

This is an inventory, **not** a claim of feature parity. The user requested
[AlistGo/alist](https://github.com/AlistGo/alist) as the reference. A shallow
checkout was inspected at commit `92e745360766cce32564f94a468cb576f577d27e`
(commit date 2026-10-21). Its license file is AGPL-3.0. Consult upstream
notices and dependency licenses before importing code or distributing binaries;
a local reference checkout is not included in this repository. Reinspect and
pin a commit when updating this audit, since upstream changes.

## What the reference contains

- The reference is a Go server (`go.mod` declares Go 1.25). Its
  `drivers/all.go` imports 90 driver packages. Package count is not a count of
  fully functional services or proof that provider APIs currently work.
- That clone's `public/dist/` only contains a placeholder, not the SolidJS
  frontend build or its source. Cloning this repo alone cannot recreate the
  upstream UI; obtain and verify the corresponding frontend separately before
  making claims about UI compatibility.
- The current Kova production server is the native C++ implementation; the
  separate Go prototype is incomplete. Native Kova registers eleven driver
  names, not all upstream drivers. Matching a driver's name does not establish
  operation, config, OAuth or data-format compatibility.
- The two servers have different architectures and databases. Do not point
  a new backend at a real Kova or AList data directory as a migration shortcut.
  Render's existing working service should not be overwritten during an
  experiment. Render Free is still ephemeral.

## Release gates for a safe migration

1. Inventory driver registration, capabilities (list, stat, range, upload,
   delete, rename, copy), settings and error behavior from a pinned upstream
   version, including any special account variants. Provider policies, auth and
   quotas need live tests; imports alone do not prove support.
2. Build API-contract tests for authentication, mount administration, listing,
   file operations, public links and WebDAV before swapping a backend. Preserve
   Kova's permissions and private-data defaults, and test authorization on
   every operation. A proxy or a shared admin token is not multi-user parity.
3. Decide whether to adapt the upstream Go server, port drivers, or maintain
   separate backends after measuring real memory, startup, transfer throughput
   and deploy constraints. Compare like-for-like workloads; no speed claim is
   justified by language choice. Check whether a durable free external database
   is actually supported before changing Render deployment guidance.
4. Design a versioned, reversible export/import on **copies** of accounts,
   mounts, secrets, settings and signing keys. Verify schema and encrypted
   credential formats, handle conflicts and run restore tests before cutover.
5. Redesign the workspace as an operating tool, retaining complete keyboard,
   mobile, screen-reader, EN/VI, light/dark, error and loading behavior. Check
   real browsers and devices, not only DOM emulators. Preserve self-hosted
   assets and restrictive CSP.
6. Ship in small, opt-in increments with regression, security and real-provider
   tests. Keep the current Render deployment as the rollback until every gate
   is met; do not claim all upstream features are present based on a clone.

## Current first increment

The native explorer caches its derived sorted/filter list for repeated renders
of the same fetched listing, filter, sort and language. It invalidates on those
inputs changing and still shows folders first. Progressive loading and view
changes no longer redo the full sort. Sort direction is exposed using
`aria-sort`. The DOM smoke test covers 10,000 sample entries, cache reuse,
filter and direction invalidation, but does not measure real-browser latency,
network speed or provider performance. A full visual redesign and upstream
backend/driver integration remain future work.
