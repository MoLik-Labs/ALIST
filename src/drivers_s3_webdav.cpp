#include "alist/common.hpp"
#include "alist/crypto.hpp"
#include "alist/driver.hpp"
#include "alist/http_client.hpp"
#include "alist/json.hpp"
#include "alist/security.hpp"
#include "alist/vfs.hpp"
#include "alist/xml.hpp"

#include <unordered_set>

namespace alist {

namespace {
struct ParsedEndpoint {
    std::string scheme = "https";
    std::string host;
    std::string base_path;
};

ParsedEndpoint parse_endpoint_url(std::string_view ep) {
    ParsedEndpoint out;
    ep = trim_ascii(ep);
    auto pos = ep.find("://");
    if (pos != std::string_view::npos) {
        out.scheme = to_lower_ascii(ep.substr(0, pos));
        ep.remove_prefix(pos + 3);
    }
    auto slash = ep.find('/');
    if (slash == std::string_view::npos) {
        out.host = std::string(ep);
        out.base_path = "";
    } else {
        out.host = std::string(ep.substr(0, slash));
        out.base_path = std::string(ep.substr(slash));
        while (out.base_path.size() > 1 && out.base_path.back() == '/') out.base_path.pop_back();
        if (out.base_path == "/") out.base_path.clear();
    }
    return out;
}
} // namespace

// ============================================================================
// 1. AWS S3 / Cloudflare R2 / MinIO / Backblaze B2 Native SigV4 Driver
// ============================================================================
class S3Driver final : public StorageDriver {
public:
    static DriverConfigInfo static_info() {
        return DriverConfigInfo{
            .name = "S3",
            .display_name = "S3 Object Storage (AWS / Cloudflare R2 / MinIO / B2)",
            .local_sort = true,
            .no_cache = false,
            .no_upload = false,
            .default_proxy = false,
            .fields = {
                {"bucket",            "Bucket Name",       "string",   "",      "", true,  "Target S3 bucket"},
                {"endpoint",          "Endpoint URL",      "string",   "https://s3.amazonaws.com", "", true, "S3/R2/MinIO endpoint"},
                {"region",            "Region",            "string",   "auto",  "", false, "AWS region or 'auto' for R2"},
                {"access_key_id",     "Access Key ID",     "string",   "",      "", true,  "S3 Access Key"},
                {"secret_access_key", "Secret Access Key", "password", "",      "", true,  "S3 Secret Key (encrypted at rest)"},
                {"root_folder_path",  "Root Prefix",       "string",   "/",     "", false, "Optional key prefix inside bucket"},
                {"force_path_style",  "Force Path Style",  "bool",     "true",  "", false, "Use endpoint/bucket/key (required for MinIO/local)"},
                {"sign_url_expire",   "Presign TTL (sec)", "number",   "3600",  "", false, "Presigned 302 URL expiration in seconds"},
                {"allow_private_net", "Allow LAN Endpoint","bool",     "false", "", false, "Enable only for trusted LAN MinIO/Garage servers"},
            }
        };
    }

    DriverConfigInfo info() const override { return static_info(); }

    bool init(const StorageRecord& record, VfsEngine* vfs, std::string& err_out) override {
        auto j = JsonValue::parse(record.addition);
        if (!j || !j->is_object()) { err_out = "Invalid S3 addition JSON"; return false; }
        bucket_ = std::string(trim_ascii(j->get("bucket").as_sv()));
        creds_.access_key = std::string(trim_ascii(j->get("access_key_id").as_sv()));
        creds_.secret_key = std::string(trim_ascii(j->get("secret_access_key").as_sv()));
        creds_.region     = std::string(trim_ascii(j->get("region").as_sv("auto")));
        if (bucket_.empty() || creds_.access_key.empty() || creds_.secret_key.empty()) {
            err_out = "S3 bucket, access_key_id, and secret_access_key are required";
            return false;
        }
        auto ep = parse_endpoint_url(j->get("endpoint").as_sv("https://s3.amazonaws.com"));
        scheme_ = ep.scheme;
        endpoint_host_ = ep.host;
        force_path_style_ = j->get("force_path_style").as_bool(true);
        presign_ttl_ = std::clamp<int64_t>(j->get("sign_url_expire").as_i64(3600), 60, 604800);
        allow_private_net_ = j->get("allow_private_net").as_bool(false) || (vfs && vfs->config().allow_private_net_outbound);
        web_proxy_ = record.web_proxy;

        std::string rp = clean_vpath(j->get("root_folder_path").as_sv("/")).value_or("/");
        root_prefix_ = (rp == "/") ? "" : rp.substr(1);
        return !endpoint_host_.empty();
    }

    std::optional<FileObj> stat(std::string_view subpath, std::string& err_out) override {
        auto clean_path = clean_vpath(subpath);
        if (!clean_path) { err_out = "Invalid S3 path"; return std::nullopt; }
        const auto& clean = *clean_path;
        if (clean == "/") {
            FileObj root;
            root.name = "/";
            root.is_dir = true;
            root.type = 1;
            root.modified = unix_now();
            return root;
        }
        std::string key = to_s3_key(clean, false);
        auto [host, uri] = build_host_and_uri(key);
        int64_t now = unix_now();
        std::string empty_hash = sha256_hex("");
        auto hdrs = aws_sigv4_sign_headers(creds_, "HEAD", host, uri, "", empty_hash, now);

        OutboundRequest req;
        req.method = "HEAD";
        req.url = scheme_ + "://" + host + uri;
        req.headers = std::move(hdrs);
        req.allow_private_net = allow_private_net_;
        auto resp = HttpClient::instance().perform(req);
        if (resp.ok && resp.status_code == 200) {
            auto [_, base] = split_vpath(clean);
            FileObj f;
            f.name = base;
            f.is_dir = false;
            if (auto it = resp.headers.find("content-length"); it != resp.headers.end()) {
                f.size = parse_i64(it->second).value_or(0);
            }
            if (auto it = resp.headers.find("etag"); it != resp.headers.end()) {
                f.etag = it->second;
            }
            f.modified = unix_now();
            f.type = classify_file_type(f.name, false);
            return f;
        }
        if (!resp.ok || resp.status_code != 404) {
            err_out = resp.error.empty() ? ("S3 HEAD HTTP " + std::to_string(resp.status_code)) : resp.error;
            return std::nullopt;
        }
        // A directory can be represented by a zero-byte marker, children, or
        // both. An empty successful listing alone is not evidence it exists.
        auto [marker_host, marker_uri] = build_host_and_uri(to_s3_key(clean, true));
        OutboundRequest marker;
        marker.method = "HEAD";
        marker.url = scheme_ + "://" + marker_host + marker_uri;
        marker.headers = aws_sigv4_sign_headers(creds_, "HEAD", marker_host, marker_uri, "", empty_hash, unix_now());
        marker.allow_private_net = allow_private_net_;
        auto marker_resp = HttpClient::instance().perform(marker);
        if (!marker_resp.ok || (marker_resp.status_code != 200 && marker_resp.status_code != 404)) {
            err_out = marker_resp.error.empty() ? ("S3 marker HEAD HTTP " + std::to_string(marker_resp.status_code)) : marker_resp.error;
            return std::nullopt;
        }
        bool exists = marker_resp.status_code == 200;
        if (!exists) {
            std::vector<FileObj> children;
            if (!list(clean, children, err_out)) return std::nullopt;
            exists = !children.empty();
        }
        if (!exists) { err_out = "S3 object not found"; return std::nullopt; }
        auto [_, base] = split_vpath(clean);
        FileObj dir;
        dir.name = base; dir.is_dir = true; dir.type = 1; dir.modified = unix_now();
        return dir;
    }

    bool list(std::string_view subpath, std::vector<FileObj>& out, std::string& err_out) override {
        auto clean = clean_vpath(subpath);
        if (!clean) { err_out = "Invalid S3 path"; return false; }
        std::string prefix = to_s3_key(*clean, true);
        auto [host, uri] = build_bucket_root();
        std::vector<FileObj> entries;
        std::unordered_set<std::string> seen_tokens;
        std::string token;
        constexpr size_t max_entries = 100000;
        constexpr size_t max_pages = 1000;

        for (size_t page = 0; page < max_pages; ++page) {
            std::map<std::string, std::string> q = {
                {"delimiter", "/"}, {"list-type", "2"}, {"max-keys", "1000"}, {"prefix", prefix},
            };
            if (!token.empty()) q["continuation-token"] = token;
            std::string canonical_q;
            for (const auto& [k, v] : q) {
                if (!canonical_q.empty()) canonical_q.push_back('&');
                canonical_q += url_encode(k) + "=" + url_encode(v);
            }
            auto hdrs = aws_sigv4_sign_headers(creds_, "GET", host, uri, canonical_q, sha256_hex(""), unix_now());
            OutboundRequest req;
            req.method = "GET";
            req.url = scheme_ + "://" + host + uri + "?" + canonical_q;
            req.headers = std::move(hdrs);
            req.allow_private_net = allow_private_net_;
            auto resp = HttpClient::instance().perform(req);
            if (!resp.ok || resp.status_code != 200) {
                err_out = resp.error.empty() ? ("S3 ListObjectsV2 HTTP " + std::to_string(resp.status_code)) : resp.error;
                return false;
            }
            auto xml = parse_xml(resp.body, &err_out);
            if (!xml || xml->local_name() != "ListBucketResult") {
                err_out = "Invalid S3 listing response";
                return false;
            }
            for (const auto* cp : xml->find_children("CommonPrefixes")) {
                std::string p = cp->child_text("Prefix");
                if (p.size() <= prefix.size() || !p.starts_with(prefix)) continue;
                std::string name = p.substr(prefix.size());
                if (!name.empty() && name.back() == '/') name.pop_back();
                if (name.empty() || name.find('/') != std::string::npos) continue;
                FileObj d;
                d.name = std::move(name); d.is_dir = true; d.type = 1; d.modified = unix_now();
                entries.push_back(std::move(d));
            }
            for (const auto* ct : xml->find_children("Contents")) {
                std::string k = ct->child_text("Key");
                if (k == prefix || k.size() <= prefix.size() || !k.starts_with(prefix)) continue;
                std::string name = k.substr(prefix.size());
                if (name.find('/') != std::string::npos) continue;
                FileObj f;
                f.name = std::move(name); f.is_dir = false;
                f.size = parse_i64(ct->child_text("Size")).value_or(0);
                f.etag = ct->child_text("ETag"); f.modified = unix_now();
                f.type = classify_file_type(f.name, false);
                entries.push_back(std::move(f));
            }
            if (entries.size() > max_entries) { err_out = "S3 listing exceeds entry limit"; return false; }
            std::string flag = xml->child_text("IsTruncated");
            auto truncated = trim_ascii(flag);
            if (truncated == "false") { out = std::move(entries); return true; }
            if (truncated != "true") { err_out = "Invalid S3 pagination flag"; return false; }
            token = xml->child_text("NextContinuationToken");
            if (token.empty() || token.size() > 8192 || !seen_tokens.insert(token).second) {
                err_out = "Invalid or repeated S3 continuation token";
                return false;
            }
        }
        err_out = "S3 listing exceeds page limit";
        return false;
    }

    std::optional<DownloadResolution> resolve_download(
        std::string_view subpath,
        const std::map<std::string, std::string>&,
        std::string& err_out) override {

        auto clean = clean_vpath(subpath).value_or("/");
        if (clean == "/") { err_out = "Cannot download root directory"; return std::nullopt; }
        std::string key = to_s3_key(clean, false);
        auto [host, uri] = build_host_and_uri(key);
        std::string presigned = aws_sigv4_presign_url(creds_, "GET", scheme_, host, uri, unix_now(), presign_ttl_);

        auto [_, base] = split_vpath(clean);
        DownloadResolution res;
        res.mode = web_proxy_ ? DownloadResolution::Mode::UpstreamHttp : DownloadResolution::Mode::RedirectUrl;
        res.url = std::move(presigned);
        res.allow_private_net = allow_private_net_;
        res.file_meta.name = base;
        res.mime_type = std::string(guess_mime_type(base));
        return res;
    }

    bool mkdir(std::string_view subpath, std::string& err_out) override {
        auto clean = clean_vpath(subpath).value_or("/");
        if (clean == "/") return true;
        std::string key = to_s3_key(clean, true);
        auto [host, uri] = build_host_and_uri(key);
        std::string empty_hash = sha256_hex("");
        auto hdrs = aws_sigv4_sign_headers(creds_, "PUT", host, uri, "", empty_hash, unix_now());

        OutboundRequest req;
        req.method = "PUT";
        req.url = scheme_ + "://" + host + uri;
        req.headers = std::move(hdrs);
        req.allow_private_net = allow_private_net_;
        auto resp = HttpClient::instance().perform(req);
        if (!resp.ok || (resp.status_code != 200 && resp.status_code != 201)) {
            err_out = "S3 mkdir failed with HTTP " + std::to_string(resp.status_code);
            return false;
        }
        return true;
    }

    bool remove(std::string_view subpath, std::string& err_out) override {
        auto clean = clean_vpath(subpath).value_or("/");
        if (clean == "/") { err_out = "Cannot delete S3 root"; return false; }
        std::string key = to_s3_key(clean, false);
        auto [host, uri] = build_host_and_uri(key);
        std::string empty_hash = sha256_hex("");
        auto hdrs = aws_sigv4_sign_headers(creds_, "DELETE", host, uri, "", empty_hash, unix_now());

        OutboundRequest req;
        req.method = "DELETE";
        req.url = scheme_ + "://" + host + uri;
        req.headers = std::move(hdrs);
        req.allow_private_net = allow_private_net_;
        auto resp = HttpClient::instance().perform(req);
        if (!resp.ok || (resp.status_code != 204 && resp.status_code != 200)) {
            err_out = "S3 delete failed with HTTP " + std::to_string(resp.status_code);
            return false;
        }
        return true;
    }

    bool copy(std::string_view src_subpath, std::string_view dst_subpath, std::string& err_out) override {
        std::string src_key = to_s3_key(clean_vpath(src_subpath).value_or("/"), false);
        std::string dst_key = to_s3_key(clean_vpath(dst_subpath).value_or("/"), false);
        if (src_key == dst_key) { err_out = "Cannot copy an S3 object onto itself"; return false; }
        auto [host, dst_uri] = build_host_and_uri(dst_key);
        std::map<std::string, std::string> extra = {
            {"x-amz-copy-source", "/" + url_encode(bucket_) + "/" + url_encode(src_key, true)}
        };
        std::string empty_hash = sha256_hex("");
        auto hdrs = aws_sigv4_sign_headers(creds_, "PUT", host, dst_uri, "", empty_hash, unix_now(), extra);

        OutboundRequest req;
        req.method = "PUT";
        req.url = scheme_ + "://" + host + dst_uri;
        req.headers = std::move(hdrs);
        req.allow_private_net = allow_private_net_;
        // S3 CopyObject may return HTTP 200 with an XML Error after the copy
        // starts. Do not delete the source on a missing, malformed or error body.
        std::string copy_response;
        constexpr size_t max_copy_response = 1024 * 1024;
        req.on_data = [&](const char* data, size_t size) {
            if (size > max_copy_response - copy_response.size()) return false;
            copy_response.append(data, size);
            return true;
        };
        auto resp = HttpClient::instance().perform(req);
        if (!resp.ok || resp.status_code != 200) {
            err_out = "S3 copy failed with HTTP " + std::to_string(resp.status_code);
            return false;
        }
        auto xml = parse_xml(copy_response);
        if (!xml || xml->local_name() != "CopyObjectResult" ||
            trim_ascii(xml->child_text("ETag")).empty()) {
            err_out = "S3 copy did not return a valid success result";
            return false;
        }
        return true;
    }

    bool rename_or_move(std::string_view src_subpath, std::string_view dst_subpath, std::string& err_out) override {
        auto src = clean_vpath(src_subpath);
        auto dst = clean_vpath(dst_subpath);
        if (!src || !dst || *src == "/" || *dst == "/") {
            err_out = "Invalid S3 source or destination";
            return false;
        }
        if (*src == *dst) return true; // Never copy and then delete the same key.
        if (!copy(*src, *dst, err_out)) return false;
        return remove(*src, err_out);
    }

    bool put(std::string_view dst_subpath, UploadStream& stream, std::string& err_out) override {
        auto clean = clean_vpath(dst_subpath).value_or("/");
        std::string key = to_s3_key(clean, false);
        auto [host, uri] = build_host_and_uri(key);
        auto hdrs = aws_sigv4_sign_headers(creds_, "PUT", host, uri, "", "UNSIGNED-PAYLOAD", unix_now(),
                                           {{"content-type", stream.mime_type.empty() ? "application/octet-stream" : stream.mime_type}});

        OutboundRequest req;
        req.method = "PUT";
        req.url = scheme_ + "://" + host + uri;
        req.headers = std::move(hdrs);
        req.upload_reader = stream.read_fn;
        req.upload_size = stream.size;
        req.allow_private_net = allow_private_net_;
        req.timeout_ms = 600000;
        auto resp = HttpClient::instance().perform(req);
        if (!resp.ok || (resp.status_code != 200 && resp.status_code != 201)) {
            err_out = resp.error.empty() ? ("S3 PUT HTTP " + std::to_string(resp.status_code)) : resp.error;
            return false;
        }
        return true;
    }

private:
    std::string scheme_ = "https";
    std::string endpoint_host_;
    std::string bucket_;
    std::string root_prefix_;
    AwsSigV4Params creds_;
    bool force_path_style_  = true;
    int64_t presign_ttl_    = 3600;
    bool allow_private_net_ = false;
    bool web_proxy_         = false;

    std::string to_s3_key(std::string_view clean_sub, bool is_dir) const {
        std::string rel = (clean_sub == "/" || clean_sub.empty()) ? "" : std::string(clean_sub.substr(1));
        std::string full = root_prefix_;
        if (!full.empty() && !rel.empty()) full += "/";
        full += rel;
        if (is_dir && !full.empty() && full.back() != '/') full.push_back('/');
        return full;
    }

    std::pair<std::string, std::string> build_bucket_root() const {
        if (force_path_style_) {
            return {endpoint_host_, "/" + url_encode(bucket_)};
        }
        return {bucket_ + "." + endpoint_host_, "/"};
    }

    std::pair<std::string, std::string> build_host_and_uri(std::string_view key) const {
        std::string enc_key = url_encode(key, true);
        if (force_path_style_) {
            return {endpoint_host_, "/" + url_encode(bucket_) + "/" + enc_key};
        }
        return {bucket_ + "." + endpoint_host_, "/" + enc_key};
    }
};

// ============================================================================
// 2. WebDAV Remote Storage Client Driver (RFC 4918)
// ============================================================================
class WebDavClientDriver final : public StorageDriver {
public:
    static DriverConfigInfo static_info() {
        return DriverConfigInfo{
            .name = "WebDAV",
            .display_name = "WebDAV Remote Storage (Nextcloud / Synology / OwnCloud / AList)",
            .local_sort = true,
            .no_cache = false,
            .no_upload = false,
            .default_proxy = true,
            .fields = {
                {"address",           "WebDAV Server URL", "string",   "https://example.com/dav", "", true, "Base WebDAV endpoint URL"},
                {"username",          "Username",          "string",   "",      "", false, "Basic Auth username"},
                {"password",          "Password / Token",  "password", "",      "", false, "Basic Auth password or Bearer token"},
                {"bearer_token",      "Bearer Token",      "password", "",      "", false, "Optional OAuth/Bearer token instead of Basic Auth"},
                {"root_folder_path",  "Remote Root Path",  "string",   "/",     "", false, "Subfolder on remote WebDAV"},
                {"allow_private_net", "Allow LAN Server",  "bool",     "false", "", false, "Enable for trusted LAN NAS WebDAV servers"},
            }
        };
    }

    DriverConfigInfo info() const override { return static_info(); }

    bool init(const StorageRecord& record, VfsEngine* vfs, std::string& err_out) override {
        auto j = JsonValue::parse(record.addition);
        if (!j || !j->is_object()) { err_out = "Invalid WebDAV addition JSON"; return false; }
        base_url_ = std::string(trim_ascii(j->get("address").as_sv()));
        while (base_url_.size() > 1 && base_url_.back() == '/') base_url_.pop_back();
        if (base_url_.empty()) { err_out = "WebDAV address is required"; return false; }
        username_ = j->get("username").as_str();
        password_ = j->get("password").as_str();
        bearer_   = j->get("bearer_token").as_str();
        root_sub_ = clean_vpath(j->get("root_folder_path").as_sv("/")).value_or("/");
        allow_private_net_ = j->get("allow_private_net").as_bool(false) || (vfs && vfs->config().allow_private_net_outbound);
        return true;
    }

    std::optional<FileObj> stat(std::string_view subpath, std::string& err_out) override {
        std::vector<FileObj> items;
        if (!propfind(subpath, "0", items, err_out) || items.empty()) {
            if (err_out.empty()) err_out = "WebDAV object not found";
            return std::nullopt;
        }
        return items.front();
    }

    bool list(std::string_view subpath, std::vector<FileObj>& out, std::string& err_out) override {
        return propfind(subpath, "1", out, err_out);
    }

    std::optional<DownloadResolution> resolve_download(
        std::string_view subpath,
        const std::map<std::string, std::string>&,
        std::string& err_out) override {

        auto st = stat(subpath, err_out);
        if (!st || st->is_dir) {
            if (err_out.empty()) err_out = "Not a downloadable file";
            return std::nullopt;
        }
        DownloadResolution res;
        res.mode = DownloadResolution::Mode::UpstreamHttp;
        res.url = build_url(subpath, false);
        res.username = username_;
        res.password = password_;
        if (!bearer_.empty()) res.headers["Authorization"] = "Bearer " + bearer_;
        res.allow_private_net = allow_private_net_;
        res.file_meta = *st;
        res.mime_type = std::string(guess_mime_type(st->name));
        return res;
    }

    bool mkdir(std::string_view subpath, std::string& err_out) override {
        OutboundRequest req = make_req("MKCOL", build_url(subpath, true));
        auto resp = HttpClient::instance().perform(req);
        if (!resp.ok || (resp.status_code != 201 && resp.status_code != 200)) {
            err_out = "WebDAV MKCOL HTTP " + std::to_string(resp.status_code);
            return false;
        }
        return true;
    }

    bool remove(std::string_view subpath, std::string& err_out) override {
        OutboundRequest req = make_req("DELETE", build_url(subpath, false));
        auto resp = HttpClient::instance().perform(req);
        if (!resp.ok || (resp.status_code != 204 && resp.status_code != 200)) {
            err_out = "WebDAV DELETE HTTP " + std::to_string(resp.status_code);
            return false;
        }
        return true;
    }

    bool rename_or_move(std::string_view src_subpath, std::string_view dst_subpath, std::string& err_out) override {
        OutboundRequest req = make_req("MOVE", build_url(src_subpath, false));
        req.headers["Destination"] = build_url(dst_subpath, false);
        req.headers["Overwrite"] = "T";
        auto resp = HttpClient::instance().perform(req);
        if (!resp.ok || (resp.status_code != 201 && resp.status_code != 204 && resp.status_code != 200)) {
            err_out = "WebDAV MOVE HTTP " + std::to_string(resp.status_code);
            return false;
        }
        return true;
    }

    bool copy(std::string_view src_subpath, std::string_view dst_subpath, std::string& err_out) override {
        OutboundRequest req = make_req("COPY", build_url(src_subpath, false));
        req.headers["Destination"] = build_url(dst_subpath, false);
        req.headers["Overwrite"] = "T";
        auto resp = HttpClient::instance().perform(req);
        if (!resp.ok || (resp.status_code != 201 && resp.status_code != 204 && resp.status_code != 200)) {
            err_out = "WebDAV COPY HTTP " + std::to_string(resp.status_code);
            return false;
        }
        return true;
    }

    bool put(std::string_view dst_subpath, UploadStream& stream, std::string& err_out) override {
        OutboundRequest req = make_req("PUT", build_url(dst_subpath, false));
        req.upload_reader = stream.read_fn;
        req.upload_size = stream.size;
        req.timeout_ms = 600000;
        auto resp = HttpClient::instance().perform(req);
        if (!resp.ok || (resp.status_code < 200 || resp.status_code >= 300)) {
            err_out = "WebDAV PUT HTTP " + std::to_string(resp.status_code);
            return false;
        }
        return true;
    }

private:
    std::string base_url_;
    std::string username_;
    std::string password_;
    std::string bearer_;
    std::string root_sub_ = "/";
    bool allow_private_net_ = false;

    std::string build_url(std::string_view subpath, bool trailing_slash) const {
        std::string full_sub = join_vpath(root_sub_, subpath).value_or("/");
        std::string u = base_url_ + url_encode(full_sub, true);
        if (trailing_slash && !u.ends_with('/')) u.push_back('/');
        return u;
    }

    OutboundRequest make_req(std::string method, std::string url) const {
        OutboundRequest r;
        r.method = std::move(method);
        r.url = std::move(url);
        r.username = username_;
        r.password = password_;
        if (!bearer_.empty()) r.headers["Authorization"] = "Bearer " + bearer_;
        r.allow_private_net = allow_private_net_;
        return r;
    }

    bool propfind(std::string_view subpath, const char* depth, std::vector<FileObj>& out, std::string& err_out) {
        OutboundRequest req = make_req("PROPFIND", build_url(subpath, subpath == "/"));
        req.headers["Depth"] = depth;
        req.headers["Content-Type"] = "application/xml; charset=utf-8";
        req.body = R"(<?xml version="1.0" encoding="utf-8"?><D:propfind xmlns:D="DAV:"><D:allprop/></D:propfind>)";
        auto resp = HttpClient::instance().perform(req);
        if (!resp.ok || (resp.status_code != 207 && resp.status_code != 200)) {
            err_out = resp.error.empty() ? ("WebDAV PROPFIND HTTP " + std::to_string(resp.status_code)) : resp.error;
            return false;
        }
        auto xml = parse_xml(resp.body, &err_out);
        if (!xml) return false;

        out.clear();
        auto responses = xml->find_children("response");
        for (size_t idx = 0; idx < responses.size(); ++idx) {
            const auto* rnode = responses[idx];
            std::string href = url_decode(rnode->child_text("href")).value_or("");
            while (href.size() > 1 && href.back() == '/') href.pop_back();
            auto [_, name] = split_vpath(href);

            // Depth "1": skip self container entry (first response matching requested directory)
            if (std::string_view(depth) == "1" && idx == 0) continue;

            const XmlNode* prop = nullptr;
            for (const auto* ps : rnode->find_children("propstat")) {
                if (const auto* p = ps->find_child("prop")) { prop = p; break; }
            }
            if (!prop) continue;

            bool is_dir = false;
            if (const auto* rt = prop->find_child("resourcetype")) {
                if (rt->find_child("collection") != nullptr) is_dir = true;
            }
            FileObj f;
            f.name = name.empty() ? "/" : name;
            f.is_dir = is_dir;
            f.size = is_dir ? 0 : parse_i64(prop->child_text("getcontentlength")).value_or(0);
            f.etag = prop->child_text("getetag");
            f.modified = unix_now();
            f.type = classify_file_type(f.name, is_dir);
            out.push_back(std::move(f));
        }
        return true;
    }
};

std::unique_ptr<StorageDriver> make_s3_driver() { return std::make_unique<S3Driver>(); }
DriverConfigInfo s3_driver_info() { return S3Driver::static_info(); }

std::unique_ptr<StorageDriver> make_webdav_driver() { return std::make_unique<WebDavClientDriver>(); }
DriverConfigInfo webdav_driver_info() { return WebDavClientDriver::static_info(); }

} // namespace alist
