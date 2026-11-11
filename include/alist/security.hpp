#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace alist {

// Canonicalize a virtual path safely:
// - Rejects NUL bytes or control characters < 0x20
// - Converts '\\' to '/'
// - Collapses '//' and '.'
// - Clamps '..' so it can NEVER escape '/'
// - Always starts with '/' and has no trailing '/' (except "/")
std::optional<std::string> clean_vpath(std::string_view raw);

// Join two virtual paths and canonicalize
std::optional<std::string> join_vpath(std::string_view base, std::string_view rel);

// Split virtual path into (parent_dir, basename). For "/", returns ("/", "").
std::pair<std::string, std::string> split_vpath(std::string_view clean_path);

// Boundary-safe subpath check: true iff target == scope or target is strictly under scope + "/"
bool is_subpath(std::string_view scope, std::string_view target) noexcept;

// Strip scope prefix from target; expects both clean and is_subpath(scope, target) == true.
std::string relative_subpath(std::string_view scope, std::string_view target);

// Safe local filesystem confinement:
// Walks components from canonical root_realpath, blocks symlink escapes unless allow_symlinks
// is explicitly true (and even then verifies resolved target if confined).
std::optional<std::string> resolve_confined_local_path(
    std::string_view root_realpath,
    std::string_view rel_subpath,
    bool allow_symlinks = false);

// Zip-Slip proof archive extraction path join
std::optional<std::string> secure_archive_join(std::string_view base_dir, std::string_view entry_name);

// Validate single filename (no '/', '\\', '..', control chars, or empty)
bool is_valid_filename(std::string_view name) noexcept;

// SSRF Protection & Outbound URL Pinning
struct ValidatedOutboundUrl {
    std::string scheme;
    std::string host;
    int port = 80;
    std::string path_and_query;
    std::string pinned_ip;
    std::string curl_resolve_entry; // "host:port:ip" for CURLOPT_RESOLVE (defeats DNS rebinding!)
};

bool is_private_or_reserved_ip(std::string_view ip_str) noexcept;
std::optional<ValidatedOutboundUrl> validate_outbound_url(
    std::string_view url,
    bool allow_private_net,
    std::string* err_out = nullptr);

// Guaranteed O(N*M) linear-space non-backtracking wildcard/glob matcher (ReDoS-proof!)
bool linear_glob_match(std::string_view pattern, std::string_view text) noexcept;
bool matches_any_hide_pattern(std::string_view newline_patterns, std::string_view filename) noexcept;

// Sharded sliding-window brute-force limiter (per IP / key)
class BruteForceProtector {
public:
    BruteForceProtector(size_t max_failures = 8, int64_t window_sec = 300, int64_t lockout_sec = 300);

    bool is_blocked(std::string_view key);
    void record_failure(std::string_view key);
    void record_success(std::string_view key);

private:
    struct Entry {
        std::deque<int64_t> failures;
        int64_t locked_until = 0;
    };
    static constexpr size_t kShards = 16;
    struct Shard {
        std::mutex mu;
        std::unordered_map<std::string, Entry> map;
    };
    std::array<Shard, kShards> shards_;
    size_t max_failures_;
    int64_t window_sec_;
    int64_t lockout_sec_;

    Shard& shard_for(std::string_view key) noexcept;
};

// Thread-safe Token Bucket Bandwidth / Rate Limiter
class TokenBucket {
public:
    explicit TokenBucket(int64_t bytes_per_sec = 0) noexcept;
    void set_rate(int64_t bytes_per_sec) noexcept;
    int64_t rate() const noexcept;
    // Blocks caller thread just enough to respect rate limit (no-op if rate <= 0)
    void consume(size_t bytes);

private:
    std::mutex mu_;
    int64_t rate_bps_ = 0;
    double tokens_ = 0.0;
    std::chrono::steady_clock::time_point last_refill_;
};

// Mask sensitive keys in JSON objects before returning to non-root logs/responses
std::string mask_sensitive_json(std::string_view raw_json);

} // namespace alist
