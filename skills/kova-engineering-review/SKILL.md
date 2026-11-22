---
name: kova-engineering-review
description: Review, optimize, and regression-test Kova's C++20 file-server backend, AList-compatible REST API and WebDAV behavior, zero-CDN embedded Workspace App, storage drivers, release builds, and container deployment. Use for security hardening, performance work, bug fixes, refactors, and release readiness.
---

# Kova engineering review

## Mission and invariants

Make evidence-led improvements without weakening compatibility or security. Preserve the established product name Kova and the `alist` executable/CLI identity. Treat the existing API, environment variables, persisted database/configuration, storage-driver interfaces, public-share links, and WebDAV behavior as compatibility surfaces unless a migration is explicitly designed and tested.

Do not propose a language rewrite merely because another language is fashionable. First identify a measured bottleneck, define a representative workload and acceptance threshold, benchmark the current implementation, then compare a small isolated prototype. A rewrite is justified only when it materially improves the measured objective while preserving semantics, security, supportability, and migration safety.

## Review loop

1. **Establish the baseline.** Check the active branch, working tree, build/test commands, current benchmark, existing API contracts, and supported toolchain. Never discard unrelated working-tree changes.
2. **Map data flow and trust boundaries.** Trace each changed input from HTTP/config/CLI through validation, identity/authorization, canonical path resolution, driver/DB calls, response serialization, and logging. Include redirects, retries, cancellation, concurrent access, and error paths.
3. **Write the failure model.** State concrete adversarial cases and invariants before editing. Prioritize authn/authz, path and symlink confinement, SSRF/redirects, archive bounds, request framing, secret handling, resource limits, cache correctness, race/lifetime behavior, and output encoding.
4. **Make the smallest coherent change.** Keep public behavior stable. Prefer explicit bounds, RAII, safe defaults, atomic/transactional updates, and clear errors. Avoid broad rewrites and avoid claiming a defense until the relevant edge cases are tested.
5. **Add regression coverage.** Test both the failing case and neighboring success cases. Include boundary values, malformed input, user/guest/admin permission differences, and concurrency if the change affects shared state. Use a live HTTP test when practical rather than testing a helper alone.
6. **Measure performance.** Use representative sizes and both warm/cold paths. Compare before/after using the same build and host. Report raw results and trade-offs; do not infer production guarantees from a single microbenchmark. Favor bounded memory and tail latency as well as throughput.
7. **Validate end to end.** Run the UI integrity and behavior checks, native tests, release build, sanitizers, CMake/Make paths, CLI smoke tests, and container configuration checks relevant to the change. Use Playwright for actual browser rendering when available; DOM emulation is supplementary, not a visual-layout claim.
8. **Self-review.** Inspect the full diff for compatibility regressions, insecure fallbacks, accidental generated artifacts, stale branding/docs, warning suppression, and tests that can pass without exercising the changed behavior. Run whitespace and clean-tree/build reproducibility checks.

## Kova-specific test expectations

- HTTP parsing: duplicate/conflicting framing, malformed encodings, header/body caps, keep-alive draining, HEAD, Range, ETag/304, gzip, and CSP hash correctness.
- Identity and authorization: anonymous/guest/member/admin, base-path jail, role scopes, hidden/password-protected paths, 2FA replay, token expiry/revocation, and public-share limits.
- Storage/VFS: longest-prefix resolution, synthesized parents, hidden files, symlinks, recursive mounts/loops, cancellation, large stream transfer, and driver-specific configuration.
- Outbound requests: URL parsing, every redirect hop, DNS rebinding, private/metadata IPs, TLS verification, bounded response sizes/timeouts, and cleanup on failure.
- ZIP/archive: traversal, symlinks, duplicate names, unsupported methods, malformed headers, CRC, compression ratio, total expanded bytes, and partial-output cleanup.
- Frontend: EN/VI parity, dynamic admin schema IDs, authorization-sensitive controls, keyboard and focus paths, upload progress/errors, share-password gates, XSS-safe rendering, and responsive/reduced-motion behavior.
- Operations: non-root container, persistent data path and permissions, health check, Render's dynamic `PORT`, Compose ports/volumes, and reproducible CMake/Make builds.

## Report format

Summarize findings by severity and file/line, explain the exploit or failure path, state the fix, and list exact commands/results. Separate verified behavior from static analysis and environment-limited checks. Never claim sanitizer, browser, release, or container validation unless that exact check completed successfully.
