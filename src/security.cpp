#include "alist/security.hpp"
#include "alist/common.hpp"
#include "alist/json.hpp"

#include <arpa/inet.h>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <thread>

namespace alist {

std::optional<std::string> clean_vpath(std::string_view raw) {
    for (unsigned char c : raw) {
        if (c < 0x20 || c == 0x7F) return std::nullopt;
    }
    std::vector<std::string_view> stack;
    size_t i = 0;
    while (i < raw.size()) {
        while (i < raw.size() && (raw[i] == '/' || raw[i] == '\\')) ++i;
        if (i >= raw.size()) break;
        size_t j = i;
        while (j < raw.size() && raw[j] != '/' && raw[j] != '\\') ++j;
        std::string_view seg = raw.substr(i, j - i);
        if (seg == ".") {
            // no-op
        } else if (seg == "..") {
            if (!stack.empty()) stack.pop_back();
        } else {
            stack.push_back(seg);
        }
        i = j;
    }
    if (stack.empty()) return std::string("/");
    std::string out;
    for (auto seg : stack) {
        out.push_back('/');
        out.append(seg);
    }
    return out;
}

std::optional<std::string> join_vpath(std::string_view base, std::string_view rel) {
    std::string combined;
    combined.reserve(base.size() + 1 + rel.size());
    combined.append(base);
    combined.push_back('/');
    combined.append(rel);
    return clean_vpath(combined);
}

std::pair<std::string, std::string> split_vpath(std::string_view clean_path) {
    if (clean_path.empty() || clean_path == "/") return {"/", ""};
    auto slash = clean_path.rfind('/');
    if (slash == std::string_view::npos) return {"/", std::string(clean_path)};
    if (slash == 0) return {"/", std::string(clean_path.substr(1))};
    return {std::string(clean_path.substr(0, slash)), std::string(clean_path.substr(slash + 1))};
}

bool is_subpath(std::string_view scope, std::string_view target) noexcept {
    if (scope.empty() || target.empty()) return false;
    if (scope == "/") return target.front() == '/';
    if (target == scope) return true;
    return target.size() > scope.size() &&
           target.starts_with(scope) &&
           target[scope.size()] == '/';
}

std::string relative_subpath(std::string_view scope, std::string_view target) {
    if (scope == "/") return std::string(target);
    if (target == scope) return "/";
    if (target.size() > scope.size() && target.starts_with(scope) && target[scope.size()] == '/') {
        return std::string(target.substr(scope.size()));
    }
    return "/";
}

bool is_valid_filename(std::string_view name) noexcept {
    if (name.empty() || name.size() > 255 || name == "." || name == "..") return false;
    for (unsigned char c : name) {
        if (c < 0x20 || c == 0x7F || c == '/' || c == '\\') return false;
    }
    return true;
}

std::optional<std::string> resolve_confined_local_path(
    std::string_view root_realpath,
    std::string_view rel_subpath,
    bool allow_symlinks) {

    if (root_realpath.empty() || root_realpath.front() != '/') return std::nullopt;
    std::string base(root_realpath);
    while (base.size() > 1 && base.back() == '/') base.pop_back();

    auto cleaned = clean_vpath(rel_subpath);
    if (!cleaned) return std::nullopt;
    if (*cleaned == "/") return base;

    // Walk component-by-component and verify lstat() on every existing component!
    std::string cur = base;
    auto parts = split_sv(*cleaned, '/', true);
    for (size_t idx = 0; idx < parts.size(); ++idx) {
        std::string_view part = parts[idx];
        if (!is_valid_filename(part)) return std::nullopt;
        cur.push_back('/');
        cur.append(part);

        struct stat st{};
        if (::lstat(cur.c_str(), &st) == 0) {
            if (S_ISLNK(st.st_mode)) {
                if (!allow_symlinks) {
                    return std::nullopt; // Block symlink traversal!
                }
                char resolved[PATH_MAX];
                if (!::realpath(cur.c_str(), resolved)) return std::nullopt;
                std::string res_str(resolved);
                if (!is_subpath(base, res_str)) {
                    return std::nullopt; // Symlink points outside confined root!
                }
                cur = std::move(res_str);
            }
        }
    }

    if (!is_subpath(base, cur)) return std::nullopt;
    return cur;
}

std::optional<std::string> secure_archive_join(std::string_view base_dir, std::string_view entry_name) {
    if (entry_name.empty()) return std::nullopt;
    for (unsigned char c : entry_name) {
        if (c < 0x20 || c == 0x7F) return std::nullopt;
    }
    if (entry_name.front() == '/' || entry_name.front() == '\\') return std::nullopt;
    if (entry_name.find(':') != std::string_view::npos) {
        // Reject Windows drive letters like C:\ or alternate streams
        auto colon = entry_name.find(':');
        if (colon == 1) return std::nullopt;
    }

    std::vector<std::string_view> parts;
    size_t i = 0;
    while (i < entry_name.size()) {
        if (entry_name[i] == '/' || entry_name[i] == '\\') {
            if (i + 1 < entry_name.size() && (entry_name[i + 1] == '/' || entry_name[i + 1] == '\\') && i == 0) {
                return std::nullopt; // Reject UNC //server/share
            }
            ++i;
            continue;
        }
        size_t j = i;
        while (j < entry_name.size() && entry_name[j] != '/' && entry_name[j] != '\\') ++j;
        std::string_view seg = entry_name.substr(i, j - i);
        if (seg == "..") return std::nullopt; // Any '..' in archive entry is immediately rejected
        if (seg != ".") {
            if (!is_valid_filename(seg)) return std::nullopt;
            parts.push_back(seg);
        }
        i = j;
    }
    if (parts.empty()) return std::nullopt;

    std::string out(base_dir);
    while (out.size() > 1 && out.back() == '/') out.pop_back();
    for (auto seg : parts) {
        out.push_back('/');
        out.append(seg);
    }
    return out;
}

namespace {
bool is_private_ipv4_host_order(uint32_t ip) noexcept {
    // 0.0.0.0/8 ("this" network)
    if ((ip & 0xFF000000u) == 0x00000000u) return true;
    // 10.0.0.0/8 (RFC1918)
    if ((ip & 0xFF000000u) == 0x0A000000u) return true;
    // 100.64.0.0/10 (CGNAT)
    if ((ip & 0xFFC00000u) == 0x64400000u) return true;
    // 127.0.0.0/8 (Loopback)
    if ((ip & 0xFF000000u) == 0x7F000000u) return true;
    // 169.254.0.0/16 (Link-local & Cloud IMDS 169.254.169.254!)
    if ((ip & 0xFFFF0000u) == 0xA9FE0000u) return true;
    // 172.16.0.0/12 (RFC1918)
    if ((ip & 0xFFF00000u) == 0xAC100000u) return true;
    // 192.0.0.0/24, 192.0.2.0/24 (TEST-NET / E2B internal events 192.0.2.1!)
    if ((ip & 0xFFFFFF00u) == 0xC0000000u || (ip & 0xFFFFFF00u) == 0xC0000200u) return true;
    // 192.168.0.0/16 (RFC1918)
    if ((ip & 0xFFFF0000u) == 0xC0A80000u) return true;
    // 198.18.0.0/15 (Benchmarking)
    if ((ip & 0xFFFE0000u) == 0xC6120000u) return true;
    // 224.0.0.0/4 (Multicast) & 240.0.0.0/4 (Reserved / Broadcast)
    if ((ip & 0xE0000000u) == 0xE0000000u || (ip & 0xF0000000u) == 0xF0000000u) return true;
    return false;
}
} // namespace

bool is_private_or_reserved_ip(std::string_view ip_str) noexcept {
    std::string s(ip_str);
    if (!s.empty() && s.front() == '[' && s.back() == ']') {
        s = s.substr(1, s.size() - 2);
    }
    in_addr v4{};
    if (::inet_pton(AF_INET, s.c_str(), &v4) == 1) {
        return is_private_ipv4_host_order(ntohl(v4.s_addr));
    }
    in6_addr v6{};
    if (::inet_pton(AF_INET6, s.c_str(), &v6) == 1) {
        const uint8_t* b = v6.s6_addr;
        // Unspecified :: or Loopback ::1
        bool all_zero_prefix = true;
        for (int i = 0; i < 15; ++i) {
            if (b[i] != 0) { all_zero_prefix = false; break; }
        }
        if (all_zero_prefix && (b[15] == 0 || b[15] == 1)) return true;

        // IPv4-mapped IPv6 ::ffff:a.b.c.d (CRITICAL SSRF bypass vector!)
        bool is_v4_mapped = true;
        for (int i = 0; i < 10; ++i) if (b[i] != 0) is_v4_mapped = false;
        if (is_v4_mapped && b[10] == 0xFF && b[11] == 0xFF) {
            uint32_t v4_host = (static_cast<uint32_t>(b[12]) << 24) |
                               (static_cast<uint32_t>(b[13]) << 16) |
                               (static_cast<uint32_t>(b[14]) << 8) |
                               static_cast<uint32_t>(b[15]);
            return is_private_ipv4_host_order(v4_host);
        }
        // IPv4-compatible ::a.b.c.d
        if (is_v4_mapped && b[10] == 0x00 && b[11] == 0x00) {
            uint32_t v4_host = (static_cast<uint32_t>(b[12]) << 24) |
                               (static_cast<uint32_t>(b[13]) << 16) |
                               (static_cast<uint32_t>(b[14]) << 8) |
                               static_cast<uint32_t>(b[15]);
            return is_private_ipv4_host_order(v4_host);
        }
        // Unique Local fc00::/7
        if ((b[0] & 0xFE) == 0xFC) return true;
        // Link-Local fe80::/10
        if (b[0] == 0xFE && (b[1] & 0xC0) == 0x80) return true;
        // Multicast ff00::/8
        if (b[0] == 0xFF) return true;
        return false;
    }
    return true; // Unparseable IP is treated as unsafe
}

std::optional<ValidatedOutboundUrl> validate_outbound_url(
    std::string_view url,
    bool allow_private_net,
    std::string* err_out) {

    for (unsigned char c : url) {
        if (c <= 0x20 || c == 0x7F) {
            if (err_out) *err_out = "URL contains whitespace or control characters";
            return std::nullopt;
        }
    }
    auto scheme_sep = url.find("://");
    if (scheme_sep == std::string_view::npos) {
        if (err_out) *err_out = "Missing URL scheme";
        return std::nullopt;
    }
    std::string scheme = to_lower_ascii(url.substr(0, scheme_sep));
    if (scheme != "http" && scheme != "https" && scheme != "ftp" && scheme != "ftps" && scheme != "sftp") {
        if (err_out) *err_out = "Disallowed URL scheme: " + scheme;
        return std::nullopt;
    }

    std::string_view rest = url.substr(scheme_sep + 3);
    auto path_pos = rest.find('/');
    std::string_view authority = (path_pos == std::string_view::npos) ? rest : rest.substr(0, path_pos);
    std::string path_q = (path_pos == std::string_view::npos) ? "/" : std::string(rest.substr(path_pos));

    // Reject embedded userinfo@host in untrusted URLs unless sftp/ftp explicitly handles separate creds
    if (authority.find('@') != std::string_view::npos) {
        if (err_out) *err_out = "Embedded credentials in URL authority are disallowed";
        return std::nullopt;
    }

    std::string host;
    int port = (scheme == "https") ? 443 : (scheme == "sftp" ? 22 : (scheme == "ftp" || scheme == "ftps" ? 21 : 80));

    if (!authority.empty() && authority.front() == '[') {
        auto close_br = authority.find(']');
        if (close_br == std::string_view::npos) {
            if (err_out) *err_out = "Malformed IPv6 host";
            return std::nullopt;
        }
        host = std::string(authority.substr(1, close_br - 1));
        if (close_br + 1 < authority.size()) {
            if (authority[close_br + 1] != ':') {
                if (err_out) *err_out = "Invalid port delimiter";
                return std::nullopt;
            }
            auto p = parse_i64(authority.substr(close_br + 2));
            if (!p || *p < 1 || *p > 65535) {
                if (err_out) *err_out = "Invalid port";
                return std::nullopt;
            }
            port = static_cast<int>(*p);
        }
    } else {
        auto colon = authority.rfind(':');
        if (colon != std::string_view::npos) {
            host = std::string(authority.substr(0, colon));
            auto p = parse_i64(authority.substr(colon + 1));
            if (!p || *p < 1 || *p > 65535) {
                if (err_out) *err_out = "Invalid port";
                return std::nullopt;
            }
            port = static_cast<int>(*p);
        } else {
            host = std::string(authority);
        }
    }

    if (host.empty()) {
        if (err_out) *err_out = "Empty hostname";
        return std::nullopt;
    }

    // Resolve DNS and inspect EVERY returned A / AAAA record!
    addrinfo hints{};
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    addrinfo* res = nullptr;
    std::string port_str = std::to_string(port);
    int gai_rc = ::getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res);
    if (gai_rc != 0 || !res) {
        if (err_out) *err_out = "DNS resolution failed for host: " + host;
        return std::nullopt;
    }
    ScopeExit free_gai([&] { ::freeaddrinfo(res); });

    std::string chosen_ip;
    for (addrinfo* cur = res; cur != nullptr; cur = cur->ai_next) {
        char ip_buf[INET6_ADDRSTRLEN]{};
        if (cur->ai_family == AF_INET) {
            auto* sin = reinterpret_cast<sockaddr_in*>(cur->ai_addr);
            ::inet_ntop(AF_INET, &sin->sin_addr, ip_buf, sizeof(ip_buf));
        } else if (cur->ai_family == AF_INET6) {
            auto* sin6 = reinterpret_cast<sockaddr_in6*>(cur->ai_addr);
            ::inet_ntop(AF_INET6, &sin6->sin6_addr, ip_buf, sizeof(ip_buf));
        } else {
            continue;
        }
        std::string ip(ip_buf);
        if (!allow_private_net && is_private_or_reserved_ip(ip)) {
            if (err_out) *err_out = "SSRF blocked: host resolves to private/reserved IP (" + ip + ")";
            return std::nullopt;
        }
        if (chosen_ip.empty()) chosen_ip = std::move(ip);
    }

    if (chosen_ip.empty()) {
        if (err_out) *err_out = "No usable IP address resolved";
        return std::nullopt;
    }

    ValidatedOutboundUrl out;
    out.scheme = std::move(scheme);
    out.host = host;
    out.port = port;
    out.path_and_query = std::move(path_q);
    out.pinned_ip = chosen_ip;
    bool is_v6 = (chosen_ip.find(':') != std::string::npos);
    out.curl_resolve_entry = host + ":" + port_str + ":" + (is_v6 ? ("[" + chosen_ip + "]") : chosen_ip);
    return out;
}

bool linear_glob_match(std::string_view pattern, std::string_view text) noexcept {
    size_t p = 0, t = 0;
    size_t star_p = std::string_view::npos;
    size_t match_t = 0;

    while (t < text.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == text[t])) {
            ++p;
            ++t;
        } else if (p < pattern.size() && pattern[p] == '*') {
            star_p = p++;
            match_t = t;
        } else if (star_p != std::string_view::npos) {
            p = star_p + 1;
            t = ++match_t;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*') ++p;
    return p == pattern.size();
}

bool matches_any_hide_pattern(std::string_view newline_patterns, std::string_view filename) noexcept {
    if (newline_patterns.empty()) return false;
    auto lines = split_sv(newline_patterns, '\n', true);
    for (auto line : lines) {
        line = trim_ascii(line);
        if (line.empty()) continue;
        // Support both simple regex-like anchors (^foo$, .*bar.*) and standard globs (*, ?) safely in O(N)!
        std::string glob_pat;
        glob_pat.reserve(line.size() + 2);
        bool anchored_start = false;
        bool anchored_end = false;
        if (!line.empty() && line.front() == '^') {
            anchored_start = true;
            line.remove_prefix(1);
        }
        if (!line.empty() && line.back() == '$') {
            anchored_end = true;
            line.remove_suffix(1);
        }
        if (!anchored_start && (line.empty() || line.front() != '*')) {
            // Exact match if no regex tokens, otherwise substring if regex-style
        }
        for (size_t i = 0; i < line.size(); ++i) {
            if (line[i] == '.' && i + 1 < line.size() && line[i + 1] == '*') {
                glob_pat.push_back('*');
                ++i;
            } else if (line[i] == '\\' && i + 1 < line.size()) {
                glob_pat.push_back(line[++i]);
            } else {
                glob_pat.push_back(line[i]);
            }
        }
        if (linear_glob_match(glob_pat, filename)) return true;
        if (!anchored_start && !anchored_end && glob_pat.find('*') == std::string::npos && glob_pat == filename) {
            return true;
        }
    }
    return false;
}

BruteForceProtector::BruteForceProtector(size_t max_failures, int64_t window_sec, int64_t lockout_sec)
    : max_failures_(max_failures), window_sec_(window_sec), lockout_sec_(lockout_sec) {}

BruteForceProtector::Shard& BruteForceProtector::shard_for(std::string_view key) noexcept {
    size_t h = std::hash<std::string_view>{}(key);
    return shards_[h % kShards];
}

bool BruteForceProtector::is_blocked(std::string_view key) {
    auto& sh = shard_for(key);
    int64_t now = unix_now();
    std::lock_guard<std::mutex> lk(sh.mu);
    auto it = sh.map.find(std::string(key));
    if (it == sh.map.end()) return false;
    if (it->second.locked_until > now) return true;
    if (it->second.locked_until != 0 && it->second.locked_until <= now && it->second.failures.empty()) {
        sh.map.erase(it);
    }
    return false;
}

void BruteForceProtector::record_failure(std::string_view key) {
    auto& sh = shard_for(key);
    int64_t now = unix_now();
    std::lock_guard<std::mutex> lk(sh.mu);
    if (sh.map.size() > 8192) {
        // Bounded memory: prune stale entries
        for (auto it = sh.map.begin(); it != sh.map.end();) {
            if (it->second.locked_until < now &&
                (it->second.failures.empty() || it->second.failures.back() + window_sec_ < now)) {
                it = sh.map.erase(it);
            } else {
                ++it;
            }
        }
    }
    auto& e = sh.map[std::string(key)];
    while (!e.failures.empty() && e.failures.front() + window_sec_ < now) {
        e.failures.pop_front();
    }
    e.failures.push_back(now);
    if (e.failures.size() >= max_failures_) {
        e.locked_until = now + lockout_sec_;
        e.failures.clear();
    }
}

void BruteForceProtector::record_success(std::string_view key) {
    auto& sh = shard_for(key);
    std::lock_guard<std::mutex> lk(sh.mu);
    sh.map.erase(std::string(key));
}

TokenBucket::TokenBucket(int64_t bytes_per_sec) noexcept
    : rate_bps_(bytes_per_sec),
      tokens_(static_cast<double>(bytes_per_sec)),
      last_refill_(std::chrono::steady_clock::now()) {}

void TokenBucket::set_rate(int64_t bytes_per_sec) noexcept {
    std::lock_guard<std::mutex> lk(mu_);
    rate_bps_ = bytes_per_sec;
    tokens_ = static_cast<double>(bytes_per_sec);
    last_refill_ = std::chrono::steady_clock::now();
}

int64_t TokenBucket::rate() const noexcept {
    return rate_bps_;
}

void TokenBucket::consume(size_t bytes) {
    if (bytes == 0) return;
    std::chrono::microseconds sleep_us{0};
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (rate_bps_ <= 0) return;
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - last_refill_).count();
        last_refill_ = now;
        tokens_ = std::min<double>(static_cast<double>(rate_bps_), tokens_ + elapsed * static_cast<double>(rate_bps_));
        tokens_ -= static_cast<double>(bytes);
        if (tokens_ < 0.0) {
            double wait_sec = (-tokens_) / static_cast<double>(rate_bps_);
            sleep_us = std::chrono::microseconds(static_cast<int64_t>(wait_sec * 1e6));
        }
    }
    if (sleep_us.count() > 0) {
        std::this_thread::sleep_for(sleep_us);
    }
}

std::string mask_sensitive_json(std::string_view raw_json) {
    auto parsed = JsonValue::parse(raw_json);
    if (!parsed || !parsed->is_object()) return std::string(raw_json);
    auto& obj = parsed->as_object();
    for (auto& [k, v] : obj) {
        std::string lk = to_lower_ascii(k);
        if (lk.find("secret") != std::string::npos ||
            lk.find("password") != std::string::npos ||
            lk.find("token") != std::string::npos ||
            lk.find("private_key") != std::string::npos ||
            lk == "access_key_secret") {
            if (v.is_string() && !v.as_sv().empty()) {
                v = JsonValue("********");
            }
        }
    }
    return parsed->dump();
}

} // namespace alist
