# AList-Core (`v4.0.0-standalone`)

**AList-Core** is an independent, ground-up **C++20** standalone rewrite of AList engineered for minimal binary size, minimal RAM footprint, deterministic zero-GC latency, kernel zero-copy throughput, and defense-in-depth security.

---

## Why AList-Core? (Measured Footprint & Performance)

Benchmarked on a 2-vCPU Linux x86_64 instance (`g++ 12.2 -std=c++20 -O3 -flto -fPIE -pie -Wl,-z,relro,-z,now`):

| Metric | Legacy Go AList v3 | AList-Core (`v4.0.0-standalone`) | Improvement |
| :--- | :--- | :--- | :--- |
| **Stripped Standalone Binary** | ~130 MB | **879 KB** (includes embedded Web SPA) | **~148x smaller** |
| **Cold Idle Memory (`VmRSS`)** | ~45–80 MB | **~6.9 MB** (`7,080 KB`, zero GC) | **~8–10x lower RAM** |
| **Active Loaded Memory (`VmRSS`)** | ~180–400+ MB | **~20.7 MB** (10k FTS5 index + 512 MiB stream) | **~10x lower RAM** |
| **Local File Download Path** | Userspace copy | **Linux `sendfile64()` kernel zero-copy** | **7,090 MiB/s (59.5 Gbps)** |
| **HTTP/1.1 Keep-Alive `epoll`** | `net/http` goroutines | **Non-blocking `epoll` + `eventfd` pool** | **123,200+ req/s (2 vCPU)** |
| **64-Shard VFS Cache** | Single mutex / shared slice | **64-shard LRU + `Singleflight` snapshots** | **9.27M lookups/s** |
| **Crypt Stream Encryption** | Userspace stream | **OpenSSL 3 EVP AES-256-GCM (64 KiB O(1) seek)** | **4,368 MiB/s (4.37 GiB/s)** |
| **Full-Text Search (10k files)** | Optional external / DB scan | **SQLite3 WAL + FTS5 (`unicode61` diacritics)** | **13,880+ queries/s (72 µs)** |

---

## Core Capabilities

1. **Unified Virtual Filesystem (`VfsEngine`)**
   - Longest-prefix storage mount resolution, synthesized virtual parent directories, `.balance` round-robin load-balanced mounts, cross-storage zero-RAM streaming copy/move, and thread-local + singleflight re-entrancy cycle guards.
2. **11 Native High-Performance Storage Drivers**
   - **Core & Enterprise Protocols**: `Local` (confined zero-copy `sendfile64`), `S3` (AWS S3 / Cloudflare R2 / MinIO / Backblaze B2 / Wasabi / Alibaba OSS with native SigV4 & presigned URLs), `WebDAV` (remote RFC 4918 client), `SFTP` (SSH2), `FTP` / `FTPS`.
   - **Major Cloud Providers**: `GoogleDrive` (Drive API v3 OAuth2), `OneDrive` (Microsoft Graph v1.0 personal/SharePoint), `Dropbox` (API v2).
   - **Zero-Overhead Virtual Drivers**: `Crypt` (transparent zero-knowledge **AES-256-GCM** 64 KiB chunked encryption + deterministic SIV `c1_` filename encryption with O(1) HTTP `Range` video seeking), `Alias` (VFS path projection), `URLTree` (virtual hierarchy over remote URLs).
3. **Full Protocol & API Suite**
   - **AList-Compatible JSON REST API** (`/api/auth/*`, `/api/me/*`, `/api/public/*`, `/api/fs/*`, `/api/share/*`, `/api/task/*`, `/api/admin/*`).
   - **RFC 4918 WebDAV Server** (`/dav/*`: `OPTIONS`, `PROPFIND`, `PROPPATCH`, `GET`, `HEAD`, streaming `PUT`, `MKCOL`, `DELETE`, `COPY`, `MOVE`, `LOCK`, `UNLOCK`).
   - **Signed Direct & Proxy Streaming** (`/d/*`, `/p/*`, `/ad/*` ZIP single-entry extraction) with RFC 7233 `Range` (`206 Partial Content` / `416`), `ETag` (`304`), and token-bucket rate limiting.
   - **Public Link Sharing** (`/s/:id`, `/sd/:id`, `/sp/:id`) with PBKDF2 passwords, expiration timestamps, and atomic SQLite `max_downloads` quota enforcement.
   - **Embedded Zero-CDN Single-Page Web UI** (`< 30 KB` pre-gzipped in binary): dark/light mode, breadcrumbs, video/audio/image/markdown/code previews, ZIP archive browser, FTS5 search modal, share manager, 2FA setup, and full Admin Console.

---

## Security Architecture & Hardening

Every subsystem was designed and verified under **AddressSanitizer (`ASan`)**, **LeakSanitizer (`LSan`)**, **UndefinedBehaviorSanitizer (`UBSan`)**, and **ThreadSanitizer (`TSan`)**:

- **Authentication & Cryptography**:
  - Passwords hashed with **PBKDF2-HMAC-SHA256** (`210,000` iterations + 16-byte CSPRNG salt).
  - Strictly pinned **`HS256` JWT** verification (`alg: none` and algorithm confusion rejected) + `pwd_ts` password-change invalidation + persistent SQLite `x_revoked_tokens` (`jti`) logout revocation + active device session eviction (`x_sessions`).
  - **RFC 6238 TOTP 2FA** with atomic SQLite `last_otp_step` replay protection (blocks reuse of the same 6-digit OTP within its 30-second window).
  - **At-Rest Cloud Credential Encryption**: all storage `addition` JSON secrets (`secret_access_key`, `refresh_token`, passwords) are encrypted at rest in SQLite via **AES-256-GCM** (`v1:enc:...`) and masked (`********`) in admin list APIs.
  - **Guest Disabled by Default**; `/d/*` and `/p/*` enforce either constant-time HMAC signature verification or full RBAC + `Meta.Password` + `Meta.Hide` checks.
- **Filesystem, Archive & Network Defenses**:
  - **Path & Symlink Confinement**: `clean_vpath` + boundary-safe `is_subpath` (`User.BasePath` jail) + `realpath` / `lstat` / `O_NOFOLLOW` confinement blocking symlink escapes outside mounted local roots.
  - **Archive Security**: `secure_archive_join` **Zip-Slip** prevention + compression-ratio / total-byte **Zip-Bomb** caps + CRC-32 integrity verification.
  - **Outbound SSRF & DNS-Rebinding Defense**: blocks loopback, RFC 1918 private networks, link-local cloud metadata (`169.254.169.254`), IPv6 ULA/loopback, and non-HTTP schemes by default, pinning validated DNS answers with `CURLOPT_RESOLVE`.
  - **HTTP & Parser Hardening**: rejects `Content-Length` + `Transfer-Encoding` request smuggling (`400`), enforces Slowloris socket timeouts, rejects XML `<!DOCTYPE`/`<!ENTITY` (**XXE-proof**), caps JSON nesting depth (`64`), replaces backtracking regexes with guaranteed **O(N·M) linear-time** glob matching (**ReDoS-proof**), and serves active downloads (`.html`, `.svg`, `.js`) with `Content-Security-Policy: default-src 'none'; sandbox`.

---

## Quick Start

### 1. Build from Source

Requires a C++20 compiler (`g++ >= 11` or `clang++ >= 14`) and `sqlite3`, `libcurl`, `openssl` (>= 3.0), and `zlib`:

```bash
# Build stripped, LTO-optimized hardened binary (./alist)
make -j"$(nproc)" release

# Run unit, adversarial security, and concurrency suites
make -j"$(nproc)" test
make -j"$(nproc)" asan   # AddressSanitizer + LeakSanitizer + UBSan
make -j"$(nproc)" tsan   # ThreadSanitizer

# Run throughput & memory footprint benchmarks
make -j"$(nproc)" bench
```

### 2. CLI Usage

```bash
# Start server (default: 0.0.0.0:5244, data dir: ./data)
./alist server --data ./data --port 5244

# Set or randomize the 'admin' password
./alist admin set 'YourStrongPassword!' --data ./data
./alist admin random --data ./data

# List configured storage mounts
./alist storage list --data ./data

# Show version & engine info
./alist version
```

### 3. Docker & Docker Compose

```bash
docker compose up -d --build
```

---

## License

Licensed under the [GNU Affero General Public License v3.0 (AGPL-3.0)](LICENSE).
