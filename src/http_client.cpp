#include "alist/http_client.hpp"
#include "alist/common.hpp"
#include "alist/security.hpp"
#include "alist/version.hpp"
#include "c_libs.hpp"

#include <cstring>

namespace alist {

namespace {

struct StreamState {
    const OutboundRequest* req = nullptr;
    OutboundResponse* resp = nullptr;
    bool headers_dispatched = false;
    bool aborted = false;
    size_t max_buffered_bytes = 32 * 1024 * 1024; // 32 MiB cap for buffered API responses
};

size_t curl_header_cb(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t total = size * nitems;
    auto* st = static_cast<StreamState*>(userdata);
    std::string_view line(buffer, total);
    if (line.starts_with("HTTP/")) {
        // New status line (e.g. after 100 Continue or redirect)
        st->resp->headers.clear();
        auto sp1 = line.find(' ');
        if (sp1 != std::string_view::npos) {
            auto rest = trim_ascii(line.substr(sp1 + 1));
            auto sp2 = rest.find(' ');
            auto code_sv = (sp2 == std::string_view::npos) ? rest : rest.substr(0, sp2);
            st->resp->status_code = static_cast<int>(parse_i64(code_sv).value_or(0));
        }
        return total;
    }
    auto colon = line.find(':');
    if (colon != std::string_view::npos) {
        std::string key = to_lower_ascii(trim_ascii(line.substr(0, colon)));
        std::string val = std::string(trim_ascii(line.substr(colon + 1)));
        st->resp->headers[std::move(key)] = std::move(val);
    }
    return total;
}

size_t curl_write_cb(char* ptr, size_t size, size_t nmemb, void* userdata) {
    size_t total = size * nmemb;
    auto* st = static_cast<StreamState*>(userdata);
    if (st->req->on_headers && !st->headers_dispatched) {
        st->headers_dispatched = true;
        st->req->on_headers(st->resp->status_code, st->resp->headers);
    }
    if (st->req->on_data) {
        if (!st->req->on_data(ptr, total)) {
            st->aborted = true;
            return 0; // Signals libcurl to abort transfer cleanly
        }
        return total;
    }
    if (st->resp->body.size() + total > st->max_buffered_bytes) {
        st->aborted = true;
        st->resp->error = "Upstream response exceeded maximum buffered size";
        return 0;
    }
    st->resp->body.append(ptr, total);
    return total;
}

size_t curl_read_cb(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t cap = size * nitems;
    auto* st = static_cast<StreamState*>(userdata);
    if (!st->req->upload_reader) return 0;
    ssize_t n = st->req->upload_reader(buffer, cap);
    if (n < 0) {
        st->aborted = true;
        return 0x10000000; // CURL_READFUNC_ABORT
    }
    return static_cast<size_t>(n);
}

} // namespace

HttpClient& HttpClient::instance() {
    static HttpClient inst;
    return inst;
}

HttpClient::HttpClient() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

HttpClient::~HttpClient() {
    std::lock_guard<std::mutex> lk(pool_mu_);
    for (void* h : pool_) {
        curl_easy_cleanup(static_cast<CURL*>(h));
    }
    pool_.clear();
}

void* HttpClient::acquire_handle() {
    std::lock_guard<std::mutex> lk(pool_mu_);
    if (!pool_.empty()) {
        void* h = pool_.back();
        pool_.pop_back();
        curl_easy_reset(static_cast<CURL*>(h));
        return h;
    }
    return curl_easy_init();
}

void HttpClient::release_handle(void* h) {
    if (!h) return;
    std::lock_guard<std::mutex> lk(pool_mu_);
    if (pool_.size() < 32) {
        pool_.push_back(h);
    } else {
        curl_easy_cleanup(static_cast<CURL*>(h));
    }
}

OutboundResponse HttpClient::perform(const OutboundRequest& req) {
    OutboundResponse resp;

    // 1. Validate URL & pin IP to defeat SSRF + DNS Rebinding!
    std::string ssrf_err;
    auto validated = validate_outbound_url(req.url, req.allow_private_net, &ssrf_err);
    if (!validated) {
        resp.error = std::move(ssrf_err);
        return resp;
    }

    CURL* curl = static_cast<CURL*>(acquire_handle());
    if (!curl) {
        resp.error = "Failed to initialize CURL handle";
        return resp;
    }

    curl_slist* hdr_list = nullptr;
    curl_slist* resolve_list = nullptr;
    curl_slist* quote_list = nullptr;
    ScopeExit cleanup([&] {
        if (hdr_list) curl_slist_free_all(hdr_list);
        if (resolve_list) curl_slist_free_all(resolve_list);
        if (quote_list) curl_slist_free_all(quote_list);
        release_handle(curl);
    });

    resolve_list = curl_slist_append(resolve_list, validated->curl_resolve_entry.c_str());
    curl_easy_setopt(curl, CURLOPT_RESOLVE, resolve_list);
    curl_easy_setopt(curl, CURLOPT_URL, req.url.c_str());
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L);
    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 131072L); // 128 KiB streaming chunks
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(req.connect_timeout_ms));
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(req.timeout_ms));
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https,ftp,ftps,sftp");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "AList-nggkhaiz/4.1");
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, req.insecure_tls ? 0L : 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, req.insecure_tls ? 0L : 2L);

    // Note: Only follow redirects automatically when private net is allowed;
    // when SSRF guard is active, we never blindly follow an open redirect to 169.254.169.254!
    if (req.follow_redirects && req.allow_private_net) {
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    } else {
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    }

    if (!req.username.empty() || !req.password.empty()) {
        curl_easy_setopt(curl, CURLOPT_USERNAME, req.username.c_str());
        curl_easy_setopt(curl, CURLOPT_PASSWORD, req.password.c_str());
    }
    if (!req.ssh_private_key_file.empty()) {
        curl_easy_setopt(curl, CURLOPT_SSH_PRIVATE_KEYFILE, req.ssh_private_key_file.c_str());
    }
    if (validated->scheme == "ftps") {
        curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
    }
    if (req.dir_list_only) {
        curl_easy_setopt(curl, CURLOPT_DIRLISTONLY, 1L);
    }
    for (const auto& q : req.quote_commands) {
        quote_list = curl_slist_append(quote_list, q.c_str());
    }
    if (quote_list) {
        curl_easy_setopt(curl, CURLOPT_QUOTE, quote_list);
    }

    for (const auto& [k, v] : req.headers) {
        std::string hline = k + ": " + v;
        hdr_list = curl_slist_append(hdr_list, hline.c_str());
    }
    // Disable Expect: 100-continue stall on PUT/POST
    hdr_list = curl_slist_append(hdr_list, "Expect:");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdr_list);

    StreamState state{&req, &resp, false, false};
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_header_cb);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &state);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &state);

    if (req.method == "HEAD") {
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    } else if (req.upload_reader) {
        curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, req.method.c_str());
        curl_easy_setopt(curl, CURLOPT_READFUNCTION, curl_read_cb);
        curl_easy_setopt(curl, CURLOPT_READDATA, &state);
        curl_easy_setopt(curl, CURLOPT_FTP_CREATE_MISSING_DIRS, 1L);
        if (req.upload_size >= 0) {
            curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(req.upload_size));
        }
    } else if (req.method == "GET") {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    } else {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, req.method.c_str());
        if (!req.body.empty() || req.method == "POST" || req.method == "PUT") {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.body.data());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(req.body.size()));
        }
    }

    char errbuf[256]{};
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

    CURLcode code = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code > 0) resp.status_code = static_cast<int>(http_code);

    if (req.on_headers && !state.headers_dispatched && code == CURLE_OK) {
        state.headers_dispatched = true;
        req.on_headers(resp.status_code, resp.headers);
    }

    if (code != CURLE_OK) {
        if (resp.error.empty()) {
            resp.error = (errbuf[0] != '\0') ? std::string(errbuf) : std::string(curl_easy_strerror(code));
        }
        resp.ok = false;
    } else {
        resp.ok = true;
    }
    return resp;
}

} // namespace alist
