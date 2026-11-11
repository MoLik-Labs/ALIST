#pragma once

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unistd.h>
#include <utility>
#include <vector>

namespace alist {

// RAII wrapper for POSIX file descriptors
class UniqueFd {
public:
    UniqueFd() noexcept = default;
    explicit UniqueFd(int fd) noexcept : fd_(fd) {}
    ~UniqueFd() noexcept { reset(); }

    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;

    UniqueFd(UniqueFd&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
    UniqueFd& operator=(UniqueFd&& other) noexcept {
        if (this != &other) {
            reset(std::exchange(other.fd_, -1));
        }
        return *this;
    }

    [[nodiscard]] int get() const noexcept { return fd_; }
    [[nodiscard]] bool valid() const noexcept { return fd_ >= 0; }
    explicit operator bool() const noexcept { return valid(); }

    int release() noexcept { return std::exchange(fd_, -1); }
    void reset(int new_fd = -1) noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
        }
        fd_ = new_fd;
    }

private:
    int fd_ = -1;
};

template <typename F>
class ScopeExit {
public:
    explicit ScopeExit(F&& fn) noexcept : fn_(std::forward<F>(fn)) {}
    ~ScopeExit() noexcept {
        if (active_) fn_();
    }
    ScopeExit(const ScopeExit&) = delete;
    ScopeExit& operator=(const ScopeExit&) = delete;
    void dismiss() noexcept { active_ = false; }

private:
    F fn_;
    bool active_ = true;
};

inline int64_t unix_now() noexcept {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

inline int64_t steady_ms() noexcept {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

inline std::string format_iso8601(int64_t unix_sec) {
    std::time_t t = static_cast<std::time_t>(unix_sec);
    std::tm tm_utc{};
    gmtime_r(&t, &tm_utc);
    char buf[80];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                  tm_utc.tm_year + 1900, tm_utc.tm_mon + 1, tm_utc.tm_mday,
                  tm_utc.tm_hour, tm_utc.tm_min, tm_utc.tm_sec);
    return std::string(buf);
}

inline std::string format_http_date(int64_t unix_sec) {
    static constexpr const char* kDays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static constexpr const char* kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                              "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    std::time_t t = static_cast<std::time_t>(unix_sec);
    std::tm tm_utc{};
    gmtime_r(&t, &tm_utc);
    if (tm_utc.tm_wday < 0 || tm_utc.tm_wday > 6 || tm_utc.tm_mon < 0 || tm_utc.tm_mon > 11) {
        return "Thu, 01 Jan 1970 00:00:00 GMT";
    }
    char buf[80];
    std::snprintf(buf, sizeof(buf), "%s, %02d %s %04d %02d:%02d:%02d GMT",
                  kDays[tm_utc.tm_wday], tm_utc.tm_mday, kMonths[tm_utc.tm_mon],
                  tm_utc.tm_year + 1900, tm_utc.tm_hour, tm_utc.tm_min, tm_utc.tm_sec);
    return std::string(buf);
}

inline std::string format_amz_date(int64_t unix_sec) {
    std::time_t t = static_cast<std::time_t>(unix_sec);
    std::tm tm_utc{};
    gmtime_r(&t, &tm_utc);
    char buf[80];
    std::snprintf(buf, sizeof(buf), "%04d%02d%02dT%02d%02d%02dZ",
                  tm_utc.tm_year + 1900, tm_utc.tm_mon + 1, tm_utc.tm_mday,
                  tm_utc.tm_hour, tm_utc.tm_min, tm_utc.tm_sec);
    return std::string(buf);
}

inline std::string to_lower_ascii(std::string_view sv) {
    std::string out(sv.size(), '\0');
    for (size_t i = 0; i < sv.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(sv[i]);
        out[i] = (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : static_cast<char>(c);
    }
    return out;
}

inline bool iequals_ascii(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        unsigned char ca = static_cast<unsigned char>(a[i]);
        unsigned char cb = static_cast<unsigned char>(b[i]);
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return false;
    }
    return true;
}

inline std::string_view trim_ascii(std::string_view sv) noexcept {
    while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t' || sv.front() == '\r' || sv.front() == '\n')) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && (sv.back() == ' ' || sv.back() == '\t' || sv.back() == '\r' || sv.back() == '\n')) {
        sv.remove_suffix(1);
    }
    return sv;
}

inline std::vector<std::string_view> split_sv(std::string_view sv, char delim, bool skip_empty = false) {
    std::vector<std::string_view> out;
    size_t start = 0;
    while (start <= sv.size()) {
        size_t pos = sv.find(delim, start);
        std::string_view part = (pos == std::string_view::npos) ? sv.substr(start) : sv.substr(start, pos - start);
        if (!skip_empty || !part.empty()) {
            out.push_back(part);
        }
        if (pos == std::string_view::npos) break;
        start = pos + 1;
    }
    return out;
}

inline std::optional<int64_t> parse_i64(std::string_view sv) noexcept {
    sv = trim_ascii(sv);
    if (sv.empty()) return std::nullopt;
    int64_t val = 0;
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
    if (ec != std::errc{} || ptr != sv.data() + sv.size()) return std::nullopt;
    return val;
}

inline std::optional<uint64_t> parse_u64(std::string_view sv) noexcept {
    sv = trim_ascii(sv);
    if (sv.empty()) return std::nullopt;
    uint64_t val = 0;
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), val);
    if (ec != std::errc{} || ptr != sv.data() + sv.size()) return std::nullopt;
    return val;
}

inline std::string url_encode(std::string_view sv, bool keep_slash = false) {
    static constexpr const char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(sv.size() + 16);
    for (unsigned char c : sv) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~' ||
            (keep_slash && c == '/')) {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(kHex[c >> 4]);
            out.push_back(kHex[c & 0x0F]);
        }
    }
    return out;
}

inline std::optional<std::string> url_decode(std::string_view sv, bool plus_as_space = false) {
    auto hex_val = [](unsigned char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::string out;
    out.reserve(sv.size());
    for (size_t i = 0; i < sv.size(); ++i) {
        char c = sv[i];
        if (c == '%') {
            if (i + 2 >= sv.size()) return std::nullopt;
            int hi = hex_val(static_cast<unsigned char>(sv[i + 1]));
            int lo = hex_val(static_cast<unsigned char>(sv[i + 2]));
            if (hi < 0 || lo < 0) return std::nullopt;
            char decoded = static_cast<char>((hi << 4) | lo);
            if (decoded == '\0') return std::nullopt; // Reject NUL byte injection
            out.push_back(decoded);
            i += 2;
        } else if (plus_as_space && c == '+') {
            out.push_back(' ');
        } else {
            if (c == '\0') return std::nullopt;
            out.push_back(c);
        }
    }
    return out;
}

// File type classification matching AList v3 conventions:
// 0=unknown, 1=folder, 2=office, 3=video, 4=audio, 5=text, 6=image, 7=archive
inline int classify_file_type(std::string_view name, bool is_dir) noexcept {
    if (is_dir) return 1;
    auto dot = name.rfind('.');
    if (dot == std::string_view::npos || dot + 1 >= name.size()) return 0;
    std::string ext = to_lower_ascii(name.substr(dot + 1));
    if (ext == "mp4" || ext == "mkv" || ext == "webm" || ext == "mov" || ext == "avi" || ext == "m3u8" || ext == "ts" || ext == "flv") return 3;
    if (ext == "mp3" || ext == "flac" || ext == "wav" || ext == "ogg" || ext == "m4a" || ext == "aac" || ext == "opus") return 4;
    if (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "gif" || ext == "webp" || ext == "svg" || ext == "bmp" || ext == "ico" || ext == "avif") return 6;
    if (ext == "txt" || ext == "md" || ext == "markdown" || ext == "json" || ext == "yaml" || ext == "yml" ||
        ext == "toml" || ext == "xml" || ext == "csv" || ext == "log" || ext == "ini" || ext == "conf" ||
        ext == "cpp" || ext == "hpp" || ext == "c" || ext == "h" || ext == "go" || ext == "rs" || ext == "py" ||
        ext == "js" || ext == "ts" || ext == "tsx" || ext == "jsx" || ext == "html" || ext == "css" || ext == "sh" || ext == "sql") return 5;
    if (ext == "pdf" || ext == "doc" || ext == "docx" || ext == "xls" || ext == "xlsx" || ext == "ppt" || ext == "pptx") return 2;
    if (ext == "zip" || ext == "tar" || ext == "gz" || ext == "tgz" || ext == "7z" || ext == "rar" || ext == "xz" || ext == "zst") return 7;
    return 0;
}

inline std::string_view guess_mime_type(std::string_view name) noexcept {
    auto dot = name.rfind('.');
    if (dot == std::string_view::npos || dot + 1 >= name.size()) return "application/octet-stream";
    std::string ext = to_lower_ascii(name.substr(dot + 1));
    if (ext == "html" || ext == "htm") return "text/html; charset=utf-8";
    if (ext == "css") return "text/css; charset=utf-8";
    if (ext == "js" || ext == "mjs") return "application/javascript; charset=utf-8";
    if (ext == "json") return "application/json; charset=utf-8";
    if (ext == "txt" || ext == "log" || ext == "conf" || ext == "ini") return "text/plain; charset=utf-8";
    if (ext == "md" || ext == "markdown") return "text/markdown; charset=utf-8";
    if (ext == "xml") return "application/xml; charset=utf-8";
    if (ext == "svg") return "image/svg+xml";
    if (ext == "png") return "image/png";
    if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
    if (ext == "gif") return "image/gif";
    if (ext == "webp") return "image/webp";
    if (ext == "ico") return "image/x-icon";
    if (ext == "avif") return "image/avif";
    if (ext == "mp4") return "video/mp4";
    if (ext == "webm") return "video/webm";
    if (ext == "mkv") return "video/x-matroska";
    if (ext == "mov") return "video/quicktime";
    if (ext == "mp3") return "audio/mpeg";
    if (ext == "flac") return "audio/flac";
    if (ext == "wav") return "audio/wav";
    if (ext == "ogg") return "audio/ogg";
    if (ext == "m4a") return "audio/mp4";
    if (ext == "pdf") return "application/pdf";
    if (ext == "zip") return "application/zip";
    if (ext == "gz" || ext == "tgz") return "application/gzip";
    if (ext == "wasm") return "application/wasm";
    return "application/octet-stream";
}

inline bool is_potentially_active_web_content(std::string_view name) noexcept {
    auto dot = name.rfind('.');
    if (dot == std::string_view::npos) return false;
    std::string ext = to_lower_ascii(name.substr(dot + 1));
    return ext == "html" || ext == "htm" || ext == "xhtml" || ext == "svg" || ext == "xml" || ext == "swf" || ext == "mhtml";
}

} // namespace alist
