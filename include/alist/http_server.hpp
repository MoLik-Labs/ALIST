#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace alist {

class TokenBucket;

struct HttpRange {
    int64_t start = 0; // inclusive
    int64_t end   = 0; // inclusive
};

// Parse RFC 7233 single byte range against total_size.
// Returns nullopt if no Range header; sets *unsatisfiable = true if Range is invalid (416).
std::optional<HttpRange> parse_http_range(std::string_view range_header, int64_t total_size, bool* unsatisfiable = nullptr);

struct HttpRequest {
    std::string method;
    std::string raw_target; // e.g. "/api/fs/list?refresh=true"
    std::string path;       // URL-decoded path component
    std::string raw_path;   // Raw encoded path component
    std::string query;
    std::string http_version = "HTTP/1.1";
    std::string remote_ip;
    bool trusted_proxy = false;
    std::map<std::string, std::string> headers; // lowercased keys
    std::map<std::string, std::string> query_params;

    // Populated for non-streaming endpoints
    std::string body;

    // Populated for streaming PUT/upload endpoints (zero-RAM buffering!)
    int64_t content_length = 0;
    bool is_chunked = false;
    std::function<ssize_t(char* buf, size_t max_len)> stream_body;

    [[nodiscard]] std::string header(std::string_view lower_key, std::string_view def = "") const {
        auto it = headers.find(std::string(lower_key));
        return (it != headers.end()) ? it->second : std::string(def);
    }

    [[nodiscard]] std::string query_param(std::string_view key, std::string_view def = "") const {
        auto it = query_params.find(std::string(key));
        return (it != query_params.end()) ? it->second : std::string(def);
    }

    [[nodiscard]] std::string base_url() const {
        std::string proto = "http";
        if (trusted_proxy) {
            std::string xfp = header("x-forwarded-proto");
            auto comma = xfp.find(',');
            if (comma != std::string::npos) xfp.resize(comma);
            if (xfp == "https" || xfp == "HTTPS") proto = "https";
            else if (xfp == "http" || xfp == "HTTP") proto = "http";
        }
        std::string raw_host;
        if (trusted_proxy) {
            raw_host = header("x-forwarded-host");
            auto comma = raw_host.find(',');
            if (comma != std::string::npos) raw_host.resize(comma);
        }
        if (raw_host.empty()) raw_host = header("host");
        // Strip leading/trailing spaces and strictly allow RFC 3986 host[:port] characters only
        size_t a = 0, b = raw_host.size();
        while (a < b && (raw_host[a] == ' ' || raw_host[a] == '\t')) ++a;
        while (b > a && (raw_host[b - 1] == ' ' || raw_host[b - 1] == '\t')) --b;
        std::string_view hv(raw_host.data() + a, b - a);
        bool valid_host = !hv.empty() && hv.size() <= 255;
        for (unsigned char ch : hv) {
            bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                      (ch >= '0' && ch <= '9') || ch == '.' || ch == '-' ||
                      ch == ':' || ch == '[' || ch == ']';
            if (!ok) { valid_host = false; break; }
        }
        return proto + "://" + (valid_host ? std::string(hv) : std::string("localhost"));
    }
};

struct HttpResponse {
    int status = 200;
    std::string status_text;
    std::map<std::string, std::string> headers;
    std::string body;
    bool pre_compressed = false;

    // Optional Zero-Copy Local File transfer (sendfile64 + Range + ETag!)
    std::string sendfile_path;
    int64_t sendfile_size  = 0;
    int64_t sendfile_mtime = 0;
    std::string sendfile_etag;

    // Optional Direct Custom Stream Writer: receives a sink(data, len) -> bool
    int64_t stream_content_length = -1; // >=0 known Content-Length, -1 HTTP chunked
    std::function<bool(const std::function<bool(const char*, size_t)>& sink)> stream_writer;

    void set_json(int code, std::string json_body) {
        status = code;
        headers["Content-Type"] = "application/json; charset=utf-8";
        body = std::move(json_body);
    }

    void redirect(int code, std::string location) {
        status = code;
        headers["Location"] = std::move(location);
        body.clear();
    }
};

using HttpHandler = std::function<void(HttpRequest& req, HttpResponse& resp)>;
// Predicate returning true if request body must be streamed directly rather than buffered
using StreamPredicate = std::function<bool(const HttpRequest& req_head)>;

class HttpServer {
public:
    HttpServer(std::string bind_addr, int port, int worker_threads = 0, int max_connections = 4096);
    ~HttpServer();

    HttpServer(const HttpServer&) = delete;
    HttpServer& operator=(const HttpServer&) = delete;

    struct Stats {
        uint64_t total_requests = 0;
        uint64_t bytes_sent = 0;
        uint64_t bytes_received = 0;
        int active_connections = 0;
        int64_t started_at_unix = 0;
    };

    void set_handler(HttpHandler handler) { handler_ = std::move(handler); }
    void set_stream_predicate(StreamPredicate pred) { stream_pred_ = std::move(pred); }
    void set_download_limiter(TokenBucket* limiter) { download_limiter_ = limiter; }
    void set_trust_proxy(bool trust) noexcept { trust_proxy_ = trust; }

    bool start();
    void stop();
    [[nodiscard]] int bound_port() const noexcept { return bound_port_; }
    [[nodiscard]] bool is_running() const noexcept { return running_.load(std::memory_order_relaxed); }
    [[nodiscard]] Stats stats() const noexcept {
        return Stats{
            total_requests_.load(std::memory_order_relaxed),
            bytes_sent_.load(std::memory_order_relaxed),
            bytes_received_.load(std::memory_order_relaxed),
            active_conns_.load(std::memory_order_relaxed),
            started_at_unix_.load(std::memory_order_relaxed),
        };
    }

private:
    void accept_loop();
    void worker_loop();
    void handle_connection(int client_fd, std::string remote_ip);

    std::string bind_addr_;
    int port_ = 5244;
    int bound_port_ = 0;
    int num_workers_ = 8;
    int max_connections_ = 4096;
    bool trust_proxy_ = false;

    int listen_fd_ = -1;
    int epoll_fd_  = -1;
    int wakeup_fd_ = -1;

    std::atomic<bool> running_{false};
    std::atomic<int> active_conns_{0};
    std::atomic<uint64_t> total_requests_{0};
    std::atomic<uint64_t> bytes_sent_{0};
    std::atomic<uint64_t> bytes_received_{0};
    std::atomic<int64_t> started_at_unix_{0};
    std::thread accept_thread_;
    std::vector<std::thread> workers_;

    struct ConnItem {
        int fd = -1;
        std::string ip;
    };
    std::mutex queue_mu_;
    std::condition_variable queue_cv_;
    std::deque<ConnItem> conn_queue_;

    HttpHandler handler_;
    StreamPredicate stream_pred_;
    TokenBucket* download_limiter_ = nullptr;
};

} // namespace alist
