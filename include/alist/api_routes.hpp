#pragma once

#include "alist/config.hpp"
#include "alist/db.hpp"
#include "alist/http_server.hpp"
#include "alist/security.hpp"
#include "alist/tasks.hpp"
#include "alist/vfs.hpp"
#include "alist/webdav_server.hpp"

#include <string>

namespace alist {

class ApiRouter {
public:
    ApiRouter(AppConfig& cfg, Database& db, VfsEngine& vfs, TaskManager& tasks);

    // Returns true for routes whose request body must be streamed (zero-RAM upload)
    bool should_stream_request_body(const HttpRequest& req_head) const;

    // Main HTTP dispatcher
    void dispatch(HttpRequest& req, HttpResponse& resp);

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
    void handle_direct_or_proxy(HttpRequest& req, HttpResponse& resp, bool force_proxy);
    void handle_archive_extract(HttpRequest& req, HttpResponse& resp);
    void handle_share_download(HttpRequest& req, HttpResponse& resp, bool force_proxy);

    AppConfig& cfg_;
    Database& db_;
    VfsEngine& vfs_;
    TaskManager& tasks_;

    BruteForceProtector login_bf_{8, 300, 300};
    BruteForceProtector share_bf_{12, 300, 300};
    TokenBucket down_limiter_;
    TokenBucket up_limiter_;
    WebDavServer webdav_;

    std::string precompressed_ui_gzip_;
};

} // namespace alist
