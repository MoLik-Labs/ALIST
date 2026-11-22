#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace alist {

struct OutboundRequest {
    std::string method = "GET";
    std::string url;
    std::map<std::string, std::string> headers;
    std::string body;

    // Optional streaming upload callback: returns bytes read (0 = EOF, <0 = abort)
    std::function<ssize_t(char* buf, size_t max_len)> upload_reader;
    int64_t upload_size = -1;

    // Optional streaming download callbacks (zero-RAM buffering for large files!)
    std::function<void(int status_code, const std::map<std::string, std::string>& resp_headers)> on_headers;
    std::function<bool(const char* data, size_t len)> on_data;
    std::function<bool()> should_abort;

    // Auth & transport options
    std::string username;
    std::string password;
    std::string ssh_private_key_file;
    std::vector<std::string> quote_commands; // For FTP/SFTP MKDIR/RENAME/RM
    bool dir_list_only    = false;
    bool follow_redirects = false;
    bool allow_private_net = false; // SSRF guard!
    bool insecure_tls     = false;
    int connect_timeout_ms = 10000;
    int timeout_ms         = 120000;
};

struct OutboundResponse {
    bool ok = false;
    int status_code = 0;
    std::string error;
    std::map<std::string, std::string> headers; // lowercased header names
    std::string body; // populated when on_data is not set
};

class HttpClient {
public:
    static HttpClient& instance();

    OutboundResponse perform(const OutboundRequest& req);

private:
    HttpClient();
    ~HttpClient();

    void* acquire_handle();
    void release_handle(void* h);

    std::mutex pool_mu_;
    std::vector<void*> pool_;
};

} // namespace alist
