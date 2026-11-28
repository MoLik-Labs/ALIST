#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace alist {

struct AppConfig {
    std::string data_dir   = "data";
    std::string db_file    = "data/data.db";
    std::string temp_dir   = "data/temp";
    std::string log_file   = "data/log/alist.log";
    bool log_enable        = true;
    bool debug             = false;

    std::string address    = "0.0.0.0";
    int http_port          = 5244;
    std::string site_url   = "";
    std::string jwt_secret = "";
    int token_expires_in   = 48; // hours

    int worker_threads     = 0;  // 0 = auto (hardware_concurrency, clamped 4..32)
    int task_workers       = 4;
    int max_connections    = 4096;
    int64_t max_upload_bytes = 50LL * 1024 * 1024 * 1024; // 50 GiB streaming upload cap
    int64_t download_rate_limit_bps = 0; // 0 = unlimited
    int64_t upload_rate_limit_bps   = 0;

    bool allow_private_net_outbound = false; // SSRF guard default: block private/IMDS IPs!
    bool tls_insecure_skip_verify   = false;
    std::string virustotal_api_key; // environment only; never persisted or exposed
    bool enable_mcp                 = false; // opt-in, read-only MCP HTTP endpoint
    bool trust_proxy                = false; // Only trust X-Forwarded-For when behind trusted reverse proxy

    std::vector<std::string> cors_allow_origins = {"*"};
    std::vector<std::string> cors_allow_methods = {
        "GET", "POST", "PUT", "DELETE", "PATCH", "OPTIONS", "HEAD",
        "PROPFIND", "PROPPATCH", "MKCOL", "COPY", "MOVE", "LOCK", "UNLOCK"
    };
    std::vector<std::string> cors_allow_headers = {
        "Origin", "Content-Length", "Content-Type", "Authorization", "Password",
        "File-Path", "As-Task", "Overwrite", "Range", "If-None-Match", "If-Modified-Since",
        "Depth", "Destination", "Lock-Token", "Timeout", "If", "Client-Id", "X-Share-Token", "X-OTP-Code"
    };

    bool load_or_create(const std::string& data_dir_override);
    bool save(const std::string& config_path) const;
};

} // namespace alist
