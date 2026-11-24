# Isolated AList Go baseline

`alist/` is the complete tracked source tree from
[AlistGo/alist](https://github.com/AlistGo/alist) commit
`92e745360766cce32564f94a468cb576f577d27e` (2026-10-21), imported with
`git archive`. The upstream AGPL-3.0 license and notices are retained inside
that directory. `alist.sha256.json` records the SHA-256 of each of its 859
tracked files; `python3 scripts/check_upstream_alist.py` checks the baseline.
This checksum records the imported bytes, not a cryptographic signature from
the upstream project. Any intentional change to the baseline must update the
manifest and describe its upstream provenance and modification.

This is a separate Go 1.25 module, not the existing experimental Kova Go
prototype and not the production native server. No existing deploy image or
data directory is changed. Its `public/dist` is upstream's placeholder, not a
working frontend. The frontend is a separate project with a pinned Git
submodule and has not been imported or integrated here. The driver registrations
are present, but successful build, provider operation, security, data migration,
and UI/API compatibility must be established independently before production
use. Never point this server at existing Kova data.

A separate CI workflow compiles the original Go module and checks the CLI help;
it does not run the server, prove feature parity, or create a release image.
Use a disposable environment and separate data for further investigation.
