#include "alist/common.hpp"
#include "alist/driver.hpp"
#include "alist/http_client.hpp"
#include "alist/json.hpp"
#include "alist/security.hpp"
#include "alist/vfs.hpp"

#include <mutex>

namespace alist {

namespace {
class OAuthTokenHelper {
public:
    void configure(std::string token_url, std::string client_id, std::string client_secret, std::string refresh_token) {
        token_url_     = std::move(token_url);
        client_id_     = std::move(client_id);
        client_secret_ = std::move(client_secret);
        refresh_token_ = std::move(refresh_token);
    }

    std::string get_access_token(std::string& err_out) {
        std::lock_guard<std::mutex> lk(mu_);
        int64_t now = unix_now();
        if (!access_token_.empty() && now + 60 < expires_at_) {
            return access_token_;
        }
        OutboundRequest req;
        req.method = "POST";
        req.url = token_url_;
        req.headers["Content-Type"] = "application/x-www-form-urlencoded";
        req.body = "grant_type=refresh_token&client_id=" + url_encode(client_id_) +
                   "&client_secret=" + url_encode(client_secret_) +
                   "&refresh_token=" + url_encode(refresh_token_);
        auto resp = HttpClient::instance().perform(req);
        if (!resp.ok || resp.status_code != 200) {
            err_out = "OAuth2 token refresh failed (HTTP " + std::to_string(resp.status_code) + ")";
            return "";
        }
        auto j = JsonValue::parse(resp.body);
        if (!j || !j->is_object()) {
            err_out = "Invalid OAuth2 token JSON";
            return "";
        }
        access_token_ = j->get("access_token").as_str();
        int64_t ttl = j->get("expires_in").as_i64(3600);
        expires_at_ = now + std::max<int64_t>(120, ttl);
        if (access_token_.empty()) err_out = "Empty access_token from OAuth2 endpoint";
        return access_token_;
    }

private:
    std::mutex mu_;
    std::string token_url_;
    std::string client_id_;
    std::string client_secret_;
    std::string refresh_token_;
    std::string access_token_;
    int64_t expires_at_ = 0;
};
} // namespace

// ============================================================================
// 1. Google Drive API v3 Native Driver
// ============================================================================
class GoogleDriveDriver final : public StorageDriver {
public:
    static DriverConfigInfo static_info() {
        return DriverConfigInfo{
            .name = "GoogleDrive",
            .display_name = "Google Drive (Native REST v3)",
            .local_sort = true,
            .no_cache = false,
            .no_upload = false,
            .default_proxy = true,
            .fields = {
                {"root_folder_id", "Root Folder ID", "string",   "root", "", true, "Google Drive folder ID ('root' for My Drive)"},
                {"client_id",      "OAuth Client ID","string",   "",     "", true, "Google Cloud OAuth2 Client ID"},
                {"client_secret",  "Client Secret",  "password", "",     "", true, "Google Cloud OAuth2 Client Secret"},
                {"refresh_token",  "Refresh Token",  "password", "",     "", true, "OAuth2 Refresh Token"},
            }
        };
    }
    DriverConfigInfo info() const override { return static_info(); }

    bool init(const StorageRecord& record, VfsEngine*, std::string& err_out) override {
        auto j = JsonValue::parse(record.addition);
        if (!j || !j->is_object()) { err_out = "Invalid GoogleDrive JSON"; return false; }
        root_id_ = j->get("root_folder_id").as_str("root");
        std::string cid = j->get("client_id").as_str();
        std::string sec = j->get("client_secret").as_str();
        std::string ref = j->get("refresh_token").as_str();
        if (cid.empty() || sec.empty() || ref.empty()) {
            err_out = "GoogleDrive client_id, client_secret, and refresh_token are required";
            return false;
        }
        oauth_.configure("https://oauth2.googleapis.com/token", std::move(cid), std::move(sec), std::move(ref));
        return true;
    }

    std::optional<FileObj> stat(std::string_view subpath, std::string& err_out) override {
        return resolve_node(subpath, err_out);
    }

    bool list(std::string_view subpath, std::vector<FileObj>& out, std::string& err_out) override {
        auto dir = resolve_node(subpath, err_out);
        if (!dir || !dir->is_dir) {
            if (err_out.empty()) err_out = "GoogleDrive directory not found";
            return false;
        }
        std::string tok = oauth_.get_access_token(err_out);
        if (tok.empty()) return false;

        std::string safe_id;
        for (char c : dir->id) { if (c != '\'' && c != '\\') safe_id.push_back(c); }
        std::string q = "'" + safe_id + "' in parents and trashed = false";
        OutboundRequest req;
        req.url = "https://www.googleapis.com/drive/v3/files?pageSize=1000&supportsAllDrives=true&includeItemsFromAllDrives=true"
                  "&fields=files(id,name,mimeType,size,modifiedTime)&q=" + url_encode(q);
        req.headers["Authorization"] = "Bearer " + tok;
        auto resp = HttpClient::instance().perform(req);
        if (!resp.ok || resp.status_code != 200) {
            err_out = "GoogleDrive list HTTP " + std::to_string(resp.status_code);
            return false;
        }
        auto j = JsonValue::parse(resp.body);
        if (!j) return false;
        out.clear();
        for (const auto& item : j->get("files").as_array()) {
            FileObj f;
            f.id     = item.get("id").as_str();
            f.name   = item.get("name").as_str();
            f.is_dir = (item.get("mimeType").as_sv() == "application/vnd.google-apps.folder");
            f.size   = f.is_dir ? 0 : parse_i64(item.get("size").as_sv("0")).value_or(0);
            f.modified = unix_now();
            f.type   = classify_file_type(f.name, f.is_dir);
            out.push_back(std::move(f));
        }
        return true;
    }

    std::optional<DownloadResolution> resolve_download(
        std::string_view subpath,
        const std::map<std::string, std::string>&,
        std::string& err_out) override {
        auto f = resolve_node(subpath, err_out);
        if (!f || f->is_dir) return std::nullopt;
        std::string tok = oauth_.get_access_token(err_out);
        if (tok.empty()) return std::nullopt;

        DownloadResolution res;
        res.mode = DownloadResolution::Mode::UpstreamHttp;
        res.url = "https://www.googleapis.com/drive/v3/files/" + url_encode(f->id) + "?alt=media&supportsAllDrives=true";
        res.headers["Authorization"] = "Bearer " + tok;
        res.file_meta = *f;
        res.mime_type = std::string(guess_mime_type(f->name));
        return res;
    }

    bool remove(std::string_view subpath, std::string& err_out) override {
        auto f = resolve_node(subpath, err_out);
        if (!f || f->id == root_id_) return false;
        std::string tok = oauth_.get_access_token(err_out);
        if (tok.empty()) return false;
        OutboundRequest req;
        req.method = "DELETE";
        req.url = "https://www.googleapis.com/drive/v3/files/" + url_encode(f->id) + "?supportsAllDrives=true";
        req.headers["Authorization"] = "Bearer " + tok;
        auto resp = HttpClient::instance().perform(req);
        return resp.ok && (resp.status_code == 204 || resp.status_code == 200);
    }

private:
    std::string root_id_ = "root";
    OAuthTokenHelper oauth_;

    std::optional<FileObj> resolve_node(std::string_view subpath, std::string& err_out) {
        auto clean = clean_vpath(subpath).value_or("/");
        FileObj cur;
        cur.id = root_id_;
        cur.name = "/";
        cur.is_dir = true;
        cur.type = 1;
        if (clean == "/") return cur;

        for (auto seg : split_sv(clean, '/', true)) {
            std::vector<FileObj> children;
            // List current parent by temporary subpath walk
            std::string tok = oauth_.get_access_token(err_out);
            if (tok.empty()) return std::nullopt;
            std::string safe_name;
            for (char c : seg) {
                if (c == '\'' || c == '\\') safe_name.push_back('\\');
                safe_name.push_back(c);
            }
            std::string q = "'" + cur.id + "' in parents and name = '" + safe_name + "' and trashed = false";
            OutboundRequest req;
            req.url = "https://www.googleapis.com/drive/v3/files?pageSize=2&supportsAllDrives=true&includeItemsFromAllDrives=true"
                      "&fields=files(id,name,mimeType,size)&q=" + url_encode(q);
            req.headers["Authorization"] = "Bearer " + tok;
            auto resp = HttpClient::instance().perform(req);
            if (!resp.ok || resp.status_code != 200) {
                err_out = "GoogleDrive lookup failed";
                return std::nullopt;
            }
            auto j = JsonValue::parse(resp.body);
            if (!j || j->get("files").as_array().empty()) {
                err_out = "Object not found";
                return std::nullopt;
            }
            const auto& item = j->get("files").as_array().front();
            cur.id     = item.get("id").as_str();
            cur.name   = item.get("name").as_str();
            cur.is_dir = (item.get("mimeType").as_sv() == "application/vnd.google-apps.folder");
            cur.size   = cur.is_dir ? 0 : parse_i64(item.get("size").as_sv("0")).value_or(0);
            cur.type   = classify_file_type(cur.name, cur.is_dir);
        }
        return cur;
    }
};

// ============================================================================
// 2. Microsoft OneDrive / SharePoint Graph v1.0 Native Driver
// ============================================================================
class OneDriveDriver final : public StorageDriver {
public:
    static DriverConfigInfo static_info() {
        return DriverConfigInfo{
            .name = "OneDrive",
            .display_name = "Microsoft OneDrive / SharePoint (Graph API v1.0)",
            .local_sort = true,
            .no_cache = false,
            .no_upload = false,
            .default_proxy = false,
            .fields = {
                {"client_id",        "Client ID",      "string",   "",  "", true,  "Azure App Client ID"},
                {"client_secret",    "Client Secret",  "password", "",  "", true,  "Azure App Client Secret"},
                {"refresh_token",    "Refresh Token",  "password", "",  "", true,  "OAuth2 Refresh Token"},
                {"root_folder_path", "Root Path",      "string",   "/", "", false, "OneDrive subfolder path"},
            }
        };
    }
    DriverConfigInfo info() const override { return static_info(); }

    bool init(const StorageRecord& record, VfsEngine*, std::string& err_out) override {
        auto j = JsonValue::parse(record.addition);
        if (!j || !j->is_object()) { err_out = "Invalid OneDrive JSON"; return false; }
        std::string cid = j->get("client_id").as_str();
        std::string sec = j->get("client_secret").as_str();
        std::string ref = j->get("refresh_token").as_str();
        root_sub_ = clean_vpath(j->get("root_folder_path").as_sv("/")).value_or("/");
        if (cid.empty() || sec.empty() || ref.empty()) {
            err_out = "OneDrive client_id, client_secret, and refresh_token are required";
            return false;
        }
        oauth_.configure("https://login.microsoftonline.com/common/oauth2/v2.0/token",
                         std::move(cid), std::move(sec), std::move(ref));
        return true;
    }

    std::optional<FileObj> stat(std::string_view subpath, std::string& err_out) override {
        std::string tok = oauth_.get_access_token(err_out);
        if (tok.empty()) return std::nullopt;
        std::string full = join_vpath(root_sub_, subpath).value_or("/");
        OutboundRequest req;
        req.url = (full == "/")
                      ? "https://graph.microsoft.com/v1.0/me/drive/root"
                      : "https://graph.microsoft.com/v1.0/me/drive/root:" + url_encode(full, true);
        req.headers["Authorization"] = "Bearer " + tok;
        auto resp = HttpClient::instance().perform(req);
        if (!resp.ok || resp.status_code != 200) { err_out = "OneDrive item not found"; return std::nullopt; }
        auto j = JsonValue::parse(resp.body);
        if (!j) return std::nullopt;
        return parse_item(*j);
    }

    bool list(std::string_view subpath, std::vector<FileObj>& out, std::string& err_out) override {
        std::string tok = oauth_.get_access_token(err_out);
        if (tok.empty()) return false;
        std::string full = join_vpath(root_sub_, subpath).value_or("/");
        OutboundRequest req;
        req.url = (full == "/")
                      ? "https://graph.microsoft.com/v1.0/me/drive/root/children?$top=999"
                      : "https://graph.microsoft.com/v1.0/me/drive/root:" + url_encode(full, true) + ":/children?$top=999";
        req.headers["Authorization"] = "Bearer " + tok;
        auto resp = HttpClient::instance().perform(req);
        if (!resp.ok || resp.status_code != 200) {
            err_out = "OneDrive list HTTP " + std::to_string(resp.status_code);
            return false;
        }
        auto j = JsonValue::parse(resp.body);
        if (!j) return false;
        out.clear();
        for (const auto& item : j->get("value").as_array()) {
            out.push_back(parse_item(item));
        }
        return true;
    }

    std::optional<DownloadResolution> resolve_download(
        std::string_view subpath,
        const std::map<std::string, std::string>&,
        std::string& err_out) override {
        std::string tok = oauth_.get_access_token(err_out);
        if (tok.empty()) return std::nullopt;
        std::string full = join_vpath(root_sub_, subpath).value_or("/");
        OutboundRequest req;
        req.url = "https://graph.microsoft.com/v1.0/me/drive/root:" + url_encode(full, true);
        req.headers["Authorization"] = "Bearer " + tok;
        auto resp = HttpClient::instance().perform(req);
        if (!resp.ok || resp.status_code != 200) return std::nullopt;
        auto j = JsonValue::parse(resp.body);
        if (!j) return std::nullopt;
        std::string dl = j->get("@microsoft.graph.downloadUrl").as_str();
        if (dl.empty()) { err_out = "Missing OneDrive downloadUrl"; return std::nullopt; }
        DownloadResolution res;
        res.mode = DownloadResolution::Mode::RedirectUrl;
        res.url = std::move(dl);
        res.file_meta = parse_item(*j);
        res.mime_type = std::string(guess_mime_type(res.file_meta.name));
        return res;
    }

private:
    std::string root_sub_ = "/";
    OAuthTokenHelper oauth_;

    static FileObj parse_item(const JsonValue& item) {
        FileObj f;
        f.id     = item.get("id").as_str();
        f.name   = item.get("name").as_str();
        f.is_dir = item.contains("folder");
        f.size   = f.is_dir ? 0 : item.get("size").as_i64(0);
        f.modified = unix_now();
        f.type   = classify_file_type(f.name, f.is_dir);
        return f;
    }
};

// ============================================================================
// 3. Dropbox API v2 Native Driver
// ============================================================================
class DropboxDriver final : public StorageDriver {
public:
    static DriverConfigInfo static_info() {
        return DriverConfigInfo{
            .name = "Dropbox",
            .display_name = "Dropbox (Native API v2)",
            .local_sort = true,
            .no_cache = false,
            .no_upload = false,
            .default_proxy = false,
            .fields = {
                {"app_key",          "App Key",        "string",   "",  "", true,  "Dropbox App Key"},
                {"app_secret",       "App Secret",     "password", "",  "", true,  "Dropbox App Secret"},
                {"refresh_token",    "Refresh Token",  "password", "",  "", true,  "Dropbox OAuth2 Refresh Token"},
                {"root_folder_path", "Root Path",      "string",   "/", "", false, "Dropbox root path"},
            }
        };
    }
    DriverConfigInfo info() const override { return static_info(); }

    bool init(const StorageRecord& record, VfsEngine*, std::string& err_out) override {
        auto j = JsonValue::parse(record.addition);
        if (!j || !j->is_object()) { err_out = "Invalid Dropbox JSON"; return false; }
        std::string k = j->get("app_key").as_str();
        std::string s = j->get("app_secret").as_str();
        std::string r = j->get("refresh_token").as_str();
        root_sub_ = clean_vpath(j->get("root_folder_path").as_sv("/")).value_or("/");
        if (k.empty() || s.empty() || r.empty()) {
            err_out = "Dropbox app_key, app_secret, and refresh_token are required";
            return false;
        }
        oauth_.configure("https://api.dropboxapi.com/oauth2/token", std::move(k), std::move(s), std::move(r));
        return true;
    }

    std::optional<FileObj> stat(std::string_view subpath, std::string& err_out) override {
        std::string full = join_vpath(root_sub_, subpath).value_or("/");
        if (full == "/") {
            FileObj root; root.name = "/"; root.is_dir = true; root.type = 1; root.modified = unix_now();
            return root;
        }
        std::string tok = oauth_.get_access_token(err_out);
        if (tok.empty()) return std::nullopt;
        OutboundRequest req;
        req.method = "POST";
        req.url = "https://api.dropboxapi.com/2/files/get_metadata";
        req.headers["Authorization"] = "Bearer " + tok;
        req.headers["Content-Type"] = "application/json";
        req.body = JsonValue::object({{"path", full}}).dump();
        auto resp = HttpClient::instance().perform(req);
        if (!resp.ok || resp.status_code != 200) { err_out = "Dropbox item not found"; return std::nullopt; }
        auto j = JsonValue::parse(resp.body);
        if (!j) return std::nullopt;
        return parse_entry(*j);
    }

    bool list(std::string_view subpath, std::vector<FileObj>& out, std::string& err_out) override {
        std::string tok = oauth_.get_access_token(err_out);
        if (tok.empty()) return false;
        std::string full = join_vpath(root_sub_, subpath).value_or("/");
        OutboundRequest req;
        req.method = "POST";
        req.url = "https://api.dropboxapi.com/2/files/list_folder";
        req.headers["Authorization"] = "Bearer " + tok;
        req.headers["Content-Type"] = "application/json";
        req.body = JsonValue::object({{"path", full == "/" ? "" : full}, {"limit", 2000}}).dump();
        auto resp = HttpClient::instance().perform(req);
        if (!resp.ok || resp.status_code != 200) {
            err_out = "Dropbox list_folder HTTP " + std::to_string(resp.status_code);
            return false;
        }
        auto j = JsonValue::parse(resp.body);
        if (!j) return false;
        out.clear();
        for (const auto& e : j->get("entries").as_array()) {
            out.push_back(parse_entry(e));
        }
        return true;
    }

    std::optional<DownloadResolution> resolve_download(
        std::string_view subpath,
        const std::map<std::string, std::string>&,
        std::string& err_out) override {
        std::string tok = oauth_.get_access_token(err_out);
        if (tok.empty()) return std::nullopt;
        std::string full = join_vpath(root_sub_, subpath).value_or("/");
        OutboundRequest req;
        req.method = "POST";
        req.url = "https://api.dropboxapi.com/2/files/get_temporary_link";
        req.headers["Authorization"] = "Bearer " + tok;
        req.headers["Content-Type"] = "application/json";
        req.body = JsonValue::object({{"path", full}}).dump();
        auto resp = HttpClient::instance().perform(req);
        if (!resp.ok || resp.status_code != 200) return std::nullopt;
        auto j = JsonValue::parse(resp.body);
        if (!j) return std::nullopt;
        DownloadResolution res;
        res.mode = DownloadResolution::Mode::RedirectUrl;
        res.url = j->get("link").as_str();
        res.file_meta = parse_entry(j->get("metadata"));
        res.mime_type = std::string(guess_mime_type(res.file_meta.name));
        return res;
    }

private:
    std::string root_sub_ = "/";
    OAuthTokenHelper oauth_;

    static FileObj parse_entry(const JsonValue& e) {
        FileObj f;
        f.id = e.get("id").as_str();
        f.name = e.get("name").as_str();
        f.is_dir = (e.get(".tag").as_sv() == "folder");
        f.size = f.is_dir ? 0 : e.get("size").as_i64(0);
        f.modified = unix_now();
        f.type = classify_file_type(f.name, f.is_dir);
        return f;
    }
};

// ============================================================================
// 4. SFTP & FTP / FTPS Remote Drivers (powered by libcurl libssh2 + FTP engine)
// ============================================================================
class RemoteCurlFsDriver final : public StorageDriver {
public:
    explicit RemoteCurlFsDriver(bool is_sftp) : is_sftp_(is_sftp) {}

    static DriverConfigInfo static_info(bool sftp) {
        return DriverConfigInfo{
            .name = sftp ? "SFTP" : "FTP",
            .display_name = sftp ? "SFTP / SSH Remote Storage" : "FTP / FTPS Remote Storage",
            .local_sort = true,
            .no_cache = false,
            .no_upload = false,
            .default_proxy = true,
            .fields = {
                {"address",           "Host:Port",         "string",   sftp ? "example.com:22" : "example.com:21", "", true, "Remote server host:port"},
                {"username",          "Username",          "string",   "root",  "", true,  "Login username"},
                {"password",          "Password",          "password", "",      "", false, "Login password"},
                {"private_key_path",  "SSH Key Path",      "string",   "",      "", false, "Optional SSH private key path (SFTP)"},
                {"use_ftps",          "Explicit FTPS TLS", "bool",     "false", "", false, "Require TLS for FTP"},
                {"root_folder_path",  "Remote Root Path",  "string",   "/",     "", false, "Remote directory"},
                {"allow_private_net", "Allow LAN Host",    "bool",     "false", "", false, "Allow connecting to LAN IP"},
            }
        };
    }

    DriverConfigInfo info() const override { return static_info(is_sftp_); }

    bool init(const StorageRecord& record, VfsEngine* vfs, std::string& err_out) override {
        auto j = JsonValue::parse(record.addition);
        if (!j || !j->is_object()) { err_out = "Invalid addition JSON"; return false; }
        host_port_ = std::string(trim_ascii(j->get("address").as_sv()));
        if (host_port_.empty()) { err_out = "Server address is required"; return false; }
        username_ = j->get("username").as_str();
        password_ = j->get("password").as_str();
        ssh_key_  = j->get("private_key_path").as_str();
        use_ftps_ = j->get("use_ftps").as_bool(false);
        root_sub_ = clean_vpath(j->get("root_folder_path").as_sv("/")).value_or("/");
        allow_private_net_ = j->get("allow_private_net").as_bool(false) || (vfs && vfs->config().allow_private_net_outbound);
        return true;
    }

    std::optional<FileObj> stat(std::string_view subpath, std::string& err_out) override {
        auto clean = clean_vpath(subpath).value_or("/");
        if (clean == "/") {
            FileObj r; r.name = "/"; r.is_dir = true; r.type = 1; r.modified = unix_now();
            return r;
        }
        auto [parent, base] = split_vpath(clean);
        std::vector<FileObj> entries;
        if (!list(parent, entries, err_out)) return std::nullopt;
        for (const auto& e : entries) {
            if (e.name == base) return e;
        }
        err_out = "Remote entry not found";
        return std::nullopt;
    }

    bool list(std::string_view subpath, std::vector<FileObj>& out, std::string& err_out) override {
        OutboundRequest req;
        req.url = build_url(subpath, true);
        req.username = username_;
        req.password = password_;
        req.ssh_private_key_file = ssh_key_;
        req.allow_private_net = allow_private_net_;
        auto resp = HttpClient::instance().perform(req);
        if (!resp.ok) { err_out = resp.error; return false; }

        out.clear();
        for (auto line : split_sv(resp.body, '\n', true)) {
            line = trim_ascii(line);
            if (line.empty() || line.starts_with("total ")) continue;
            // Parse standard Unix 'ls -l' line: drwxr-xr-x 2 user grp 4096 Jan 1 12:00 filename
            auto cols = split_sv(line, ' ', true);
            if (cols.size() >= 9 && (cols[0][0] == 'd' || cols[0][0] == '-' || cols[0][0] == 'l')) {
                bool is_d = (cols[0][0] == 'd');
                int64_t sz = parse_i64(cols[4]).value_or(0);
                // Reconstruct filename after 8th column
                size_t pos = 0;
                for (int c = 0; c < 8; ++c) {
                    pos = line.find(cols[static_cast<size_t>(c)], pos) + cols[static_cast<size_t>(c)].size();
                    while (pos < line.size() && line[pos] == ' ') ++pos;
                }
                std::string name(line.substr(pos));
                if (name == "." || name == "..") continue;
                FileObj f;
                f.name = std::move(name);
                f.is_dir = is_d;
                f.size = is_d ? 0 : sz;
                f.modified = unix_now();
                f.type = classify_file_type(f.name, is_d);
                out.push_back(std::move(f));
            }
        }
        return true;
    }

    std::optional<DownloadResolution> resolve_download(
        std::string_view subpath,
        const std::map<std::string, std::string>&,
        std::string& err_out) override {
        auto st = stat(subpath, err_out);
        if (!st || st->is_dir) return std::nullopt;
        DownloadResolution res;
        res.mode = DownloadResolution::Mode::UpstreamHttp;
        res.url = build_url(subpath, false);
        res.username = username_;
        res.password = password_;
        res.ssh_key_file = ssh_key_;
        res.allow_private_net = allow_private_net_;
        res.file_meta = *st;
        res.mime_type = std::string(guess_mime_type(st->name));
        return res;
    }

    bool put(std::string_view dst_subpath, UploadStream& stream, std::string& err_out) override {
        OutboundRequest req;
        req.method = "PUT";
        req.url = build_url(dst_subpath, false);
        req.username = username_;
        req.password = password_;
        req.ssh_private_key_file = ssh_key_;
        req.upload_reader = stream.read_fn;
        req.upload_size = stream.size;
        req.allow_private_net = allow_private_net_;
        auto resp = HttpClient::instance().perform(req);
        if (!resp.ok) { err_out = resp.error; return false; }
        return true;
    }

private:
    bool is_sftp_ = true;
    std::string host_port_;
    std::string username_;
    std::string password_;
    std::string ssh_key_;
    bool use_ftps_ = false;
    std::string root_sub_ = "/";
    bool allow_private_net_ = false;

    std::string build_url(std::string_view subpath, bool dir_slash) const {
        std::string scheme = is_sftp_ ? "sftp" : (use_ftps_ ? "ftps" : "ftp");
        std::string full = join_vpath(root_sub_, subpath).value_or("/");
        std::string u = scheme + "://" + host_port_ + url_encode(full, true);
        if (dir_slash && !u.ends_with('/')) u.push_back('/');
        return u;
    }
};

std::unique_ptr<StorageDriver> make_gdrive_driver()   { return std::make_unique<GoogleDriveDriver>(); }
DriverConfigInfo gdrive_driver_info()                  { return GoogleDriveDriver::static_info(); }
std::unique_ptr<StorageDriver> make_onedrive_driver() { return std::make_unique<OneDriveDriver>(); }
DriverConfigInfo onedrive_driver_info()                { return OneDriveDriver::static_info(); }
std::unique_ptr<StorageDriver> make_dropbox_driver()  { return std::make_unique<DropboxDriver>(); }
DriverConfigInfo dropbox_driver_info()                 { return DropboxDriver::static_info(); }
std::unique_ptr<StorageDriver> make_sftp_driver()     { return std::make_unique<RemoteCurlFsDriver>(true); }
DriverConfigInfo sftp_driver_info()                    { return RemoteCurlFsDriver::static_info(true); }
std::unique_ptr<StorageDriver> make_ftp_driver()      { return std::make_unique<RemoteCurlFsDriver>(false); }
DriverConfigInfo ftp_driver_info()                     { return RemoteCurlFsDriver::static_info(false); }

} // namespace alist
