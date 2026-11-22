# Cloudflare migration: current gap analysis

This is an engineering inventory, not a statement of production support. The
current authoritative server is native C++; the independent Worker is an
experimental, separately implemented prototype. Analysis is based on the code at
the time of writing, not exhaustive verification against real providers.

## Driver inventory

`src/drivers_virtual_crypt.cpp` registers eleven driver names:

| Native driver | Independent Worker status | Important migration work |
| --- | --- | --- |
| GoogleDrive | Experimental single-account and owner-scoped read-only folder list/name filter, binary/range download and Workspace export; mocks only | OAuth consent and credential lifecycle, path mapping, writes, complete export formats, error and quota handling, permissions, real-provider tests |
| OneDrive | Not implemented | Microsoft OAuth, Graph API, pagination, redirects and writes |
| Dropbox | Disabled-by-default folder listing/download and owner-scoped small create-only upload; encrypted access-token records; mocks only | Per-user OAuth and refresh, large/resumable uploads, sharing, error handling, hardening and real-provider tests |
| S3 | Not implemented as an S3 client | SigV4, endpoints, multipart, safe redirect/SSRF policy and provider tests. The R2 binding prototype is not S3 driver parity |
| WebDAV | Not implemented | HTTP methods, XML handling, streaming and SSRF controls |
| SFTP, FTP | Not implemented | Runtime/network protocol feasibility and secure authentication must be verified; do not assume native libcurl transfers directly |
| Local | Not implemented | Worker has no persistent host filesystem equivalent; evaluate virtual storage without claiming filesystem parity |
| URLTree | Not implemented | Source validation and outbound/redirect security |
| Alias | Not implemented | Virtual mount resolution, access checks, cycle and traversal handling |
| Crypt | Not implemented | Key lifecycle, format compatibility, encrypted paths and data, security review |

The existence of a native driver does not by itself prove every file operation
works. Build an operation/capability matrix from code and provider tests before
claiming per-driver parity.

## API and isolation gaps

The native router has dozens of `/api/` routes for authentication, users,
roles, storage administration, filesystem operations, shares, tasks and public
settings, plus WebDAV. `web/app.js` calls these routes directly. The experimental
Worker exposes `/v1/...` endpoints using one shared bearer token and disabled-by-default `/v2/...` experiments using preseeded sessions and owner-scoped credentials. It is not
compatible with the Kova UI/API and has no production-ready multi-user login or account
management. A disabled pre-provisioned Google login experiment has only local/mock tests. Its Google credential belongs to one Worker configuration; anyone
with its bearer token can read that account's permitted files. It must not be
used with sensitive data or as a multi-user security boundary.

The isolated D1-oriented access module is not wired to these endpoints. Its
resource records currently do not map to provider identities, filesystem paths,
folder inheritance, or versioned object metadata. Private/public flags do not
modify cloud-provider ACLs. Before connecting any real mount, the system needs
trusted authentication, authorization for **every** metadata/content operation,
credential encryption and isolation, race-safe grant changes, link controls,
quota/abuse protection, logging redaction and recovery. A deployment operator
can change Worker code and bindings; application roles cannot hide plaintext
from that operator.

## Verification and release gates

- Existing tests include mocked Google responses, a local R2 emulator smoke
  test, SQLite access tests, and a workerd/local-D1-emulator access test. They
  do not prove real Google, Cloudflare D1/R2 or browser behavior.
- A green Cloudflare Pages upload can still deploy a wrongly configured static
  directory without Functions. Workers Builds is a separate deployment/check.
- Free tiers impose quotas and platform limits; when not feasible, fail clearly
  or defer. Do not require a paid service as a hidden prerequisite.
- Implement one provider vertically: login, connection setup, persistence,
  permission enforcement, browse/download/write, cleanup and realistic end-to-end
  tests with disposable resources. Then extend. Do not wire the current shared
  prototype bearer directly into multi-user UI.
- The VitePress docs are built separately for GitHub Pages. They are not the
  app backend. No merge/release should be described as complete while critical
  functionality or the relevant cloud deployment check remains unverified.
