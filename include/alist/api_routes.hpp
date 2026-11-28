#pragma once

#include "alist/config.hpp"
#include "alist/db.hpp"
#include "alist/http_server.hpp"
#include "alist/security.hpp"
#include "alist/tasks.hpp"
#include "alist/vfs.hpp"
#include "alist/webdav_server.hpp"

#include <string>
#include <mutex>
#include <chrono>

namespace alist {

class ApiRouter {
public:
    ApiRouter(AppConfig& cfg, Database& db, VfsEngine& vfs, TaskManager& tasks);

    // Returns true for routes whose request body must be streamed (zero-RAM upload)
    bool should_stream_request_body(const HttpRequest& req_head) const;

    // Main HTTP dispatcher
    void dispatch(HttpRequest& req, HttpResponse& resp);

    void set_server(const HttpServer* srv) noexcept { server_ = srv; }
    TokenBucket& download_limiter() noexcept { return down_limiter_; }

private:
    struct AuthContext {
        UserRecord user;
        std::string device_key;
        std::string jti;
        int64_t token_exp = 0;
    };

    std::optional<AuthContext> authenticate(const HttpRequest& req, HttpResponse& resp, bool allow_guest = true);
    void serve_spa(const HttpRequest& req, HttpResponse& resp) const;
    void handle_mcp(HttpRequest& req, HttpResponse& resp);
    void handle_direct_or_proxy(HttpRequest& req, HttpResponse& resp, bool force_proxy);
    void handle_archive_extract(HttpRequest& req, HttpResponse& resp);
    void handle_share_download(HttpRequest& req, HttpResponse& resp, bool force_proxy);

    AppConfig& cfg_;
    Database& db_;
    VfsEngine& vfs_;
    TaskManager& tasks_;

    std::mutex vt_mu_;
    std::chrono::steady_clock::time_point vt_last_request_{};
    BruteForceProtector login_bf_{8, 300, 300};
    BruteForceProtector share_bf_{12, 300, 300};
    TokenBucket down_limiter_;
    TokenBucket up_limiter_;
    WebDavServer webdav_;
    const HttpServer* server_ = nullptr;
    int64_t boot_unix_ = 0;

    std::string precompressed_ui_gzip_;
    std::string ui_etag_;
    std::string ui_gzip_etag_;
    std::string ui_csp_;
    std::string dummy_pwd_hash_;
};

} // namespace alist
