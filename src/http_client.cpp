#include "alist/http_client.hpp"
#include "alist/common.hpp"
#include "alist/security.hpp"
#include "alist/version.hpp"
#include "c_libs.hpp"

#include <cstring>

namespace alist {

namespace {

constexpr int kMaxSafeRedirects = 5;

struct StreamState {
    const OutboundRequest* req = nullptr;
    OutboundResponse* resp = nullptr;
    bool suppress_body_callbacks = false;
    bool headers_dispatched = false;
    bool aborted = false;
    size_t max_buffered_bytes = 32 * 1024 * 1024; // 32 MiB cap for buffered API responses
};

std::string strip_crlf(std::string_view sv) {
    std::string out;
    out.reserve(sv.size());
    for (char c : sv) {
        if (c != '\r' && c != '\n') out.push_back(c);
    }
    return out;
}

bool is_redirect_status(int code) noexcept {
    return code == 301 || code == 302 || code == 303 || code == 307 || code == 308;
}

std::string resolve_redirect_url(const ValidatedOutboundUrl& base, std::string_view location) {
    location = trim_ascii(location);
    if (location.empty()) return {};
    if (location.starts_with("http://") || location.starts_with("https://")) {
        return std::string(location);
    }
    std::string origin = base.scheme + "://" + base.host;
    bool default_port = (base.scheme == "http" && base.port == 80) ||
                        (base.scheme == "https" && base.port == 443);
    if (!default_port && base.port > 0) {
        origin += ":" + std::to_string(base.port);
    }
    if (location.starts_with("//")) {
        return base.scheme + ":" + std::string(location);
    }
    if (location.front() == '/') {
        return origin + std::string(location);
    }
    std::string_view pq = base.path_and_query;
    auto q = pq.find('?');
    if (q != std::string_view::npos) pq = pq.substr(0, q);
    auto sl = pq.rfind('/');
    std::string dir = (sl == std::string_view::npos) ? "/" : std::string(pq.substr(0, sl + 1));
    return origin + dir + std::string(location);
}

size_t curl_header_cb(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t total = size * nitems;
    auto* st = static_cast<StreamState*>(userdata);
    std::string_view line(buffer, total);
    if (line.starts_with("HTTP/")) {
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
    // Discard body of intermediate 3xx redirect responses when hop-by-hop redirect following is active
    if (st->suppress_body_callbacks && is_redirect_status(st->resp->status_code) &&
        st->resp->headers.contains("location")) {
        return total;
    }
    if (st->req->on_headers && !st->headers_dispatched) {
        st->headers_dispatched = true;
        st->req->on_headers(st->resp->status_code, st->resp->headers);
    }
    if (st->req->on_data) {
        if (!st->req->on_data(ptr, total)) {
            st->aborted = true;
            return 0;
        }
        return total;
    }
    if (total > st->max_buffered_bytes - st->resp->body.size()) {
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

bool request_canceled(const OutboundRequest& req) noexcept {
    if (!req.should_abort) return false;
    try {
        return req.should_abort();
    } catch (...) {
        // A failing cancellation hook must stop I/O rather than escape through libcurl.
        return true;
    }
}

int curl_progress_cb(void* userdata, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    auto* st = static_cast<StreamState*>(userdata);
    if (!request_canceled(*st->req)) return 0;
    st->aborted = true;
    return 1;
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
    std::string cur_url = req.url;
    std::string cur_method = req.method;
    std::string cur_body = req.body;
    std::map<std::string, std::string> cur_headers = req.headers;
    std::string cur_user = req.username;
    std::string cur_pass = req.password;
    const bool redirect_safe_method = iequals_ascii(cur_method, "GET") || iequals_ascii(cur_method, "HEAD");
    bool can_redirect = req.follow_redirects && redirect_safe_method && !req.upload_reader;

    std::string origin_scheme;
    std::string origin_host;
    int origin_port = 0;

    for (int hop = 0; hop <= kMaxSafeRedirects; ++hop) {
        OutboundResponse resp;
        if (request_canceled(req)) {
            resp.error = "Outbound request canceled";
            return resp;
        }

        // Validate URL & DNS-pin IP on EVERY hop to defeat SSRF + open-redirect + DNS rebinding
        std::string ssrf_err;
        auto validated = validate_outbound_url(cur_url, req.allow_private_net, &ssrf_err);
        if (!validated) {
            resp.error = std::move(ssrf_err);
            return resp;
        }
        if (hop == 0) {
            origin_scheme = validated->scheme;
            origin_host = to_lower_ascii(validated->host);
            origin_port = validated->port;
        } else {
            // Strip ambient credentials on cross-origin redirect hops
            bool same_origin = (validated->scheme == origin_scheme &&
                                to_lower_ascii(validated->host) == origin_host &&
                                validated->port == origin_port);
            if (!same_origin) {
                for (auto it = cur_headers.begin(); it != cur_headers.end();) {
                    const std::string name = to_lower_ascii(it->first);
                    // Cross-origin hops keep only representation preferences. This also
                    // removes non-standard credentials (API keys, signed headers, Host, etc.).
                    const bool safe_header = name == "accept" || name == "accept-encoding" ||
                                             name == "accept-language" || name == "user-agent" ||
                                             name == "range";
                    if (!safe_header) it = cur_headers.erase(it);
                    else ++it;
                }
                cur_user.clear();
                cur_pass.clear();
            }
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
        curl_easy_setopt(curl, CURLOPT_URL, cur_url.c_str());
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L);
        curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 131072L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(req.connect_timeout_ms));
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(req.timeout_ms));
        curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, hop == 0 ? "http,https,ftp,ftps,sftp" : "http,https");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L); // Handled hop-by-hop above
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Kova/4.2");
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, req.insecure_tls ? 0L : 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, req.insecure_tls ? 0L : 2L);

        if (!cur_user.empty() || !cur_pass.empty()) {
            curl_easy_setopt(curl, CURLOPT_USERNAME, cur_user.c_str());
            curl_easy_setopt(curl, CURLOPT_PASSWORD, cur_pass.c_str());
        }
        if (hop == 0 && !req.ssh_private_key_file.empty()) {
            curl_easy_setopt(curl, CURLOPT_SSH_PRIVATE_KEYFILE, req.ssh_private_key_file.c_str());
        }
        if (validated->scheme == "ftps") {
            curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
        }
        if (hop == 0 && req.dir_list_only) {
            curl_easy_setopt(curl, CURLOPT_DIRLISTONLY, 1L);
        }
        if (hop == 0) {
            for (const auto& q : req.quote_commands) {
                quote_list = curl_slist_append(quote_list, q.c_str());
            }
            if (quote_list) curl_easy_setopt(curl, CURLOPT_QUOTE, quote_list);
        }

        for (const auto& [k, v] : cur_headers) {
            std::string sk = strip_crlf(k);
            if (sk.empty()) continue;
            std::string hline = sk + ": " + strip_crlf(v);
            hdr_list = curl_slist_append(hdr_list, hline.c_str());
        }
        hdr_list = curl_slist_append(hdr_list, "Expect:");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hdr_list);

        bool allow_more_hops = can_redirect && hop < kMaxSafeRedirects &&
                               (validated->scheme == "http" || validated->scheme == "https");
        StreamState state{&req, &resp, allow_more_hops, false, false};
        if (req.should_abort) {
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curl_progress_cb);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &state);
        }
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_header_cb);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &state);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &state);

        if (cur_method == "HEAD") {
            curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        } else if (hop == 0 && req.upload_reader) {
            curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, cur_method.c_str());
            curl_easy_setopt(curl, CURLOPT_READFUNCTION, curl_read_cb);
            curl_easy_setopt(curl, CURLOPT_READDATA, &state);
            curl_easy_setopt(curl, CURLOPT_FTP_CREATE_MISSING_DIRS, 1L);
            if (req.upload_size >= 0) {
                curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(req.upload_size));
            }
        } else if (cur_method == "GET") {
            curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
        } else {
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, cur_method.c_str());
            if (!cur_body.empty() || cur_method == "POST" || cur_method == "PUT") {
                curl_easy_setopt(curl, CURLOPT_POSTFIELDS, cur_body.data());
                curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(cur_body.size()));
            }
        }

        char errbuf[256]{};
        curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

        CURLcode code = curl_easy_perform(curl);
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        if (http_code > 0) resp.status_code = static_cast<int>(http_code);

        if (code == CURLE_OK && allow_more_hops && is_redirect_status(resp.status_code)) {
            if (auto loc_it = resp.headers.find("location"); loc_it != resp.headers.end() && !loc_it->second.empty()) {
                std::string next_url = resolve_redirect_url(*validated, loc_it->second);
                if (!next_url.empty()) {
                    cur_url = std::move(next_url);
                    if (resp.status_code == 303 ||
                        ((resp.status_code == 301 || resp.status_code == 302) && cur_method == "POST")) {
                        cur_method = "GET";
                        cur_body.clear();
                    }
                    continue;
                }
            }
        }

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

    OutboundResponse fail;
    fail.error = "Exceeded maximum redirect limit";
    return fail;
}

} // namespace alist
