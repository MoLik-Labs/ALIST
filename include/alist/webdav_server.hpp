#pragma once

#include "alist/http_server.hpp"
#include "alist/security.hpp"
#include "alist/vfs.hpp"

#include <mutex>
#include <unordered_map>

namespace alist {

class WebDavServer {
public:
    WebDavServer(VfsEngine& vfs, BruteForceProtector& bf, TokenBucket& up_limiter);

    void handle(HttpRequest& req, HttpResponse& resp);

    // Helper shared with API routes to deliver DownloadResolution into HttpResponse
    static void serve_resolution(const HttpRequest& req, HttpResponse& resp,
                                 const DownloadResolution& dl, bool force_proxy,
                                 bool force_attachment = false);

private:
    std::optional<UserRecord> authenticate_basic_or_bearer(const HttpRequest& req, HttpResponse& resp);

    VfsEngine& vfs_;
    BruteForceProtector& bf_;
    TokenBucket& up_limiter_;

    struct DavLock {
        std::string token;
        std::string owner;
        int64_t expires_at = 0;
    };
    std::mutex locks_mu_;
    std::unordered_map<std::string, DavLock> locks_;
};

} // namespace alist
