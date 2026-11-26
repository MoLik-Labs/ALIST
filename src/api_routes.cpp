#include "alist/api_routes.hpp"
#include "alist/archive.hpp"
#include "alist/common.hpp"
#include "alist/crypto.hpp"
#include "alist/json.hpp"
#include "alist/http_client.hpp"
#include "alist/version.hpp"
#include "alist/web_ui.hpp"

#include <algorithm>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <limits>
#include <thread>
#include <unistd.h>

namespace alist {

namespace {

int bounded_json_int(const JsonValue& value, int fallback, int min_value, int max_value) {
    const int64_t raw = value.as_i64(fallback);
    return static_cast<int>(std::clamp<int64_t>(raw, min_value, max_value));
}

// The admin status integration accepts only a workers.dev origin, not a URL
// supplied by an anonymous client. The outbound client additionally validates
// and pins public DNS, refuses redirects, and never forwards credentials.
bool valid_monitor_origin(std::string_view origin) {
    constexpr std::string_view scheme = "https://";
    constexpr std::string_view suffix = ".workers.dev";
    if (!origin.starts_with(scheme) || !origin.ends_with(suffix) || origin.size() > 150) return false;
    auto labels = origin.substr(scheme.size(), origin.size() - scheme.size() - suffix.size());
    auto dot = labels.find('.');
    if (dot == std::string_view::npos || labels.find('.', dot + 1) != std::string_view::npos) return false;
    auto valid_label = [](std::string_view label) {
        if (label.empty() || label.size() > 63 || label.front() == '-' || label.back() == '-') return false;
        return std::all_of(label.begin(), label.end(), [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
        });
    };
    return valid_label(labels.substr(0, dot)) && valid_label(labels.substr(dot + 1));
}

struct ProcTelemetry {
    int64_t rss_bytes = 0;
    int64_t peak_rss_bytes = 0;
    int threads = 1;
};

ProcTelemetry read_proc_telemetry() {
    ProcTelemetry t;
    std::ifstream ifs("/proc/self/status");
    std::string line;
    while (std::getline(ifs, line)) {
        if (line.starts_with("VmRSS:")) {
            auto sv = trim_ascii(std::string_view(line).substr(6));
            auto sp = sv.find(' ');
            if (auto kb = parse_i64(sp == std::string_view::npos ? sv : sv.substr(0, sp))) {
                t.rss_bytes = *kb * 1024;
            }
        } else if (line.starts_with("VmHWM:")) {
            auto sv = trim_ascii(std::string_view(line).substr(6));
            auto sp = sv.find(' ');
            if (auto kb = parse_i64(sp == std::string_view::npos ? sv : sv.substr(0, sp))) {
                t.peak_rss_bytes = *kb * 1024;
            }
        } else if (line.starts_with("Threads:")) {
            if (auto th = parse_i64(trim_ascii(std::string_view(line).substr(8)))) {
                t.threads = static_cast<int>(*th);
            }
        }
    }
    return t;
}

std::string merge_masked_addition(std::string_view old_plain_json, const JsonValue& incoming_add) {
    auto old_j = JsonValue::parse(old_plain_json);
    JsonValue new_j = incoming_add.is_object()
                          ? incoming_add
                          : JsonValue::parse(incoming_add.as_sv("{}")).value_or(JsonValue::object());
    if (!new_j.is_object()) return std::string(old_plain_json);
    if (old_j && old_j->is_object()) {
        for (const auto& [k, old_v] : old_j->as_object()) {
            if (new_j.contains(k) && new_j.get(k).is_string() && new_j.get(k).as_sv() == "********") {
                new_j[k] = old_v;
            }
        }
    }
    return new_j.dump();
}

void json_ok(HttpResponse& resp, JsonValue data = nullptr, std::string_view message = "success") {
    JsonValue env = JsonValue::object({
        {"code",    200},
        {"message", std::string(message)},
        {"data",    std::move(data)},
    });
    resp.set_json(200, env.dump());
}

void json_err(HttpResponse& resp, int code, std::string_view message, int http_status = 200) {
    // Note: AList v3 REST API convention returns HTTP 200 with {"code": 401/403/404/500, "message": ...}
    // while also supporting HTTP status codes when requested. Using HTTP 200 with envelope code ensures
    // 100% compatibility with official AList clients AND our SPA!
    JsonValue env = JsonValue::object({
        {"code",    code},
        {"message", std::string(message)},
        {"data",    nullptr},
    });
    resp.set_json(http_status, env.dump());
}

JsonValue parse_body_or_empty(const HttpRequest& req) {
    if (req.body.empty()) return JsonValue::object();
    auto j = JsonValue::parse(req.body);
    return (j && (j->is_object() || j->is_array())) ? std::move(*j) : JsonValue::object();
}

std::string share_token_payload(const ShareRecord& sh) {
    return sh.share_id + ":" + sh.password_hash + ":" + std::to_string(sh.updated_at);
}

} // namespace

ApiRouter::ApiRouter(AppConfig& cfg, Database& db, VfsEngine& vfs, TaskManager& tasks)
    : cfg_(cfg),
      db_(db),
      vfs_(vfs),
      tasks_(tasks),
      down_limiter_(cfg.download_rate_limit_bps),
      up_limiter_(cfg.upload_rate_limit_bps),
      webdav_(vfs, login_bf_, up_limiter_),
      boot_unix_(unix_now()),
      dummy_pwd_hash_(hash_password_pbkdf2("kova-timing-pad-constant")) {
    if (auto gz = gzip_compress(kEmbeddedWebUiHtml, 9)) {
        precompressed_ui_gzip_ = std::move(*gz);
    }
    ui_etag_ = "\"" + sha256_hex(kEmbeddedWebUiHtml).substr(0, 24) + "\"";
    if (!precompressed_ui_gzip_.empty()) {
        ui_gzip_etag_ = "\"" + sha256_hex(precompressed_ui_gzip_).substr(0, 24) + "\"";
    }
    // Compute exact SHA-256 CSP digest of the embedded <script> block so injected inline scripts/handlers are blocked
    std::string script_src = "'self'";
    auto s_open = kEmbeddedWebUiHtml.find("<script>");
    auto s_close = (s_open != std::string_view::npos)
                       ? kEmbeddedWebUiHtml.find("</script>", s_open + 8)
                       : std::string_view::npos;
    if (s_open != std::string_view::npos && s_close != std::string_view::npos &&
        kEmbeddedWebUiHtml.find("<script", s_open + 8) == std::string_view::npos &&
        kEmbeddedWebUiHtml.find(" onclick=") == std::string_view::npos) {
        std::string_view script_body = kEmbeddedWebUiHtml.substr(s_open + 8, s_close - (s_open + 8));
        auto digest = sha256_bytes(script_body);
        std::string_view dsv(reinterpret_cast<const char*>(digest.data()), digest.size());
        script_src += " 'sha256-" + base64_encode(dsv) + "'";
    } else {
        // Fail closed: if the bundle shape is unexpected, do not grant inline script execution.
        // The Web UI integrity check catches this before release builds.
    }
    ui_csp_ = "default-src 'self'; script-src " + script_src +
              "; style-src 'self' 'unsafe-inline'; img-src 'self' data: blob: https: http:; "
              "media-src 'self' blob: https: http:; connect-src 'self'; font-src 'self' data:; "
              "frame-src 'self' blob:; frame-ancestors 'self'; object-src 'none'; base-uri 'self'; form-action 'self'";
}

bool ApiRouter::should_stream_request_body(const HttpRequest& req_head) const {
    if (req_head.method == "PUT") {
        if (req_head.path == "/api/fs/put" || req_head.path == "/api/fs/form" ||
            req_head.path == "/dav" || req_head.path.starts_with("/dav/")) {
            return true;
        }
    }
    return false;
}

void ApiRouter::serve_spa(const HttpRequest& req, HttpResponse& resp) const {
    resp.status = 200;
    resp.headers["Content-Type"] = "text/html; charset=utf-8";
    resp.headers["Cache-Control"] = "no-cache";
    resp.headers["Referrer-Policy"] = "strict-origin-when-cross-origin";
    resp.headers["Content-Security-Policy"] = ui_csp_;

    auto& vary = resp.headers["Vary"];
    bool varies_by_encoding = false;
    for (auto item : split_sv(vary, ',')) {
        if (iequals_ascii(trim_ascii(item), "accept-encoding")) {
            varies_by_encoding = true;
            break;
        }
    }
    if (!varies_by_encoding) {
        if (!vary.empty()) vary += ", ";
        vary += "Accept-Encoding";
    }

    const bool use_gzip = !precompressed_ui_gzip_.empty() &&
                          accepts_gzip_encoding(req.header("accept-encoding"));
    if (use_gzip) {
        resp.body = precompressed_ui_gzip_;
        resp.headers["Content-Encoding"] = "gzip";
        resp.headers["ETag"] = ui_gzip_etag_;
        resp.pre_compressed = true;
    } else {
        resp.body.assign(kEmbeddedWebUiHtml);
        resp.headers["ETag"] = ui_etag_;
    }
}

std::optional<ApiRouter::AuthContext> ApiRouter::authenticate(
    const HttpRequest& req, HttpResponse& resp, bool allow_guest) {

    std::string raw_auth = req.header("authorization");
    std::string_view tok_sv = trim_ascii(raw_auth);
    if (tok_sv.size() > 7 && (tok_sv.starts_with("Bearer ") || tok_sv.starts_with("bearer "))) {
        tok_sv = trim_ascii(tok_sv.substr(7));
    }
    std::string token(tok_sv);
    std::string client_id = req.header("client-id");
    if (client_id.empty()) client_id = req.query_param("client_id", "default");

    auto attach_session = [&](UserRecord u, std::string jti = "", int64_t exp = 0) -> std::optional<AuthContext> {
        std::string dev_key = sha256_hex(std::to_string(u.id) + ":" + client_id).substr(0, 32);
        std::string serr;
        if (!db_.check_or_touch_session(u.id, dev_key, req.header("user-agent"), req.remote_ip, false, &serr)) {
            json_err(resp, 401, serr.empty() ? "Session inactive" : serr);
            return std::nullopt;
        }
        return AuthContext{std::move(u), std::move(dev_key), std::move(jti), exp};
    };

    // 1. Static Admin System Token (constant-time comparison!)
    std::string sys_token = db_.get_setting("token", "");
    if (!token.empty() && !sys_token.empty() && constant_time_equals(token, sys_token)) {
        auto admin = db_.get_admin_user();
        if (!admin) {
            json_err(resp, 500, "Admin account missing");
            return std::nullopt;
        }
        return attach_session(std::move(*admin));
    }

    // 2. Empty token -> Guest fallback if allowed & enabled
    if (token.empty()) {
        if (!allow_guest) {
            json_err(resp, 401, "Authentication required");
            return std::nullopt;
        }
        auto guest = db_.get_guest_user();
        if (!guest || guest->disabled) {
            json_err(resp, 401, "Guest access is disabled, please sign in");
            return std::nullopt;
        }
        return attach_session(std::move(*guest));
    }

    // 3. HS256 JWT Verification
    std::string jwt_err;
    auto claims = jwt_decode_hs256(token, cfg_.jwt_secret, &jwt_err);
    if (!claims) {
        json_err(resp, 401, jwt_err.empty() ? "Invalid token" : jwt_err);
        return std::nullopt;
    }
    if (db_.is_token_jti_revoked(claims->jti)) {
        json_err(resp, 401, "Token has been logged out or revoked");
        return std::nullopt;
    }
    auto u = db_.get_user_by_name(claims->username);
    if (!u || u->disabled) {
        json_err(resp, 401, "Account not found or disabled");
        return std::nullopt;
    }
    if (u->pwd_ts != claims->pwd_ts) {
        json_err(resp, 401, "Password was changed, please sign in again");
        return std::nullopt;
    }
    return attach_session(std::move(*u), claims->jti, claims->exp);
}

void ApiRouter::handle_direct_or_proxy(HttpRequest& req, HttpResponse& resp, bool force_proxy) {
    std::string_view raw_sub = std::string_view(req.path).substr(2); // strip "/d" or "/p"
    auto clean_opt = clean_vpath(raw_sub);
    if (!clean_opt || *clean_opt == "/") {
        resp.set_json(400, R"({"code":400,"message":"invalid file path","data":null})");
        return;
    }
    const std::string& gvpath = *clean_opt;
    std::string sign_q = req.query_param("sign");

    // Security Gate:
    // 1. If a valid HMAC signature is provided, verify it directly in constant time!
    // 2. Otherwise, if path requires signing -> 401; if path does NOT require signing,
    //    STILL enforce user/guest RBAC + Meta.Password + Meta.Hide so unauthenticated callers
    //    can NEVER bypass disabled-guest or folder passwords via /d/* or /p/*!
    std::string sys_tok = db_.get_setting("token", cfg_.jwt_secret);
    bool valid_hmac = (!sign_q.empty() && verify_url_payload(sys_tok, gvpath, sign_q));
    if (!valid_hmac) {
        if (vfs_.should_sign_path(gvpath)) {
            resp.set_json(401, R"({"code":401,"message":"invalid or expired download signature","data":null})");
            return;
        }
        auto auth = authenticate(req, resp, true);
        if (!auth) {
            resp.status = 401;
            return;
        }
        int st = 200;
        std::string rerr;
        if (!vfs_.can_read_path(auth->user, gvpath, req.query_param("pw", req.header("password")), &st, &rerr)) {
            resp.set_json(st, JsonValue::object({{"code", st}, {"message", rerr}, {"data", nullptr}}).dump());
            return;
        }
    }

    std::string derr;
    auto dl = vfs_.resolve_download(gvpath, req.headers, derr);
    if (!dl) {
        resp.set_json(404, JsonValue::object({{"code", 404}, {"message", derr.empty() ? "file not found" : derr}, {"data", nullptr}}).dump());
        return;
    }
    WebDavServer::serve_resolution(req, resp, *dl, force_proxy, false);
}

void ApiRouter::handle_archive_extract(HttpRequest& req, HttpResponse& resp) {
    std::string_view raw_sub = std::string_view(req.path).substr(3); // strip "/ad"
    auto clean_opt = clean_vpath(raw_sub);
    std::string inner = req.query_param("inner");
    if (!clean_opt || inner.empty()) {
        resp.set_json(400, R"({"code":400,"message":"missing archive path or inner entry","data":null})");
        return;
    }
    std::string sign_q = req.query_param("sign");
    std::string sig_payload = *clean_opt + "\x00" + inner;
    std::string sys_tok = db_.get_setting("token", cfg_.jwt_secret);

    if (!verify_url_payload(sys_tok, sig_payload, sign_q)) {
        auto auth = authenticate(req, resp, true);
        if (!auth) { resp.status = 401; return; }
        int32_t perms = vfs_.effective_permissions(auth->user, *clean_opt);
        if (!auth->user.is_admin() && !has_perm(perms, PermReadArchives)) {
            resp.set_json(403, R"({"code":403,"message":"archive permission denied","data":null})");
            return;
        }
        if (!vfs_.can_read_path(auth->user, *clean_opt, req.header("password"))) {
            resp.set_json(403, R"({"code":403,"message":"access denied","data":null})");
            return;
        }
    }

    std::string derr;
    auto dl = vfs_.resolve_download(*clean_opt, req.headers, derr);
    if (!dl || dl->mode != DownloadResolution::Mode::LocalFile) {
        resp.set_json(400, R"({"code":400,"message":"archive extraction requires local backing file","data":null})");
        return;
    }

    std::string zerr;
    auto data = zip_extract_entry(dl->local_path, inner, zerr);
    if (!data) {
        resp.set_json(400, JsonValue::object({{"code", 400}, {"message", zerr}, {"data", nullptr}}).dump());
        return;
    }

    auto [_, fname] = split_vpath(inner);
    resp.status = 200;
    resp.headers["Content-Type"] = is_potentially_active_web_content(fname)
                                       ? "application/octet-stream"
                                       : std::string(guess_mime_type(fname));
    resp.headers["Content-Security-Policy"] = "default-src 'none'; sandbox";
    resp.headers["Content-Disposition"] = "attachment; filename*=UTF-8''" + url_encode(fname);
    resp.body = std::move(*data);
}

void ApiRouter::handle_share_download(HttpRequest& req, HttpResponse& resp, bool force_proxy) {
    // Format: /sd/:share_id or /sd/:share_id/sub/path
    std::string_view rest = std::string_view(req.path).substr(4); // strip "/sd/" or "/sp/"
    auto slash = rest.find('/');
    std::string share_id(slash == std::string_view::npos ? rest : rest.substr(0, slash));
    std::string_view sub_sv = (slash == std::string_view::npos) ? "/" : rest.substr(slash);

    auto sh = db_.get_share(share_id);
    if (!sh || sh->disabled || (sh->expires_at > 0 && unix_now() > sh->expires_at)) {
        resp.set_json(404, R"({"code":404,"message":"share not found or expired","data":null})");
        return;
    }

    if (sh->has_password) {
        std::string tok = req.query_param("token", req.header("x-share-token"));
        std::string secret = db_.get_setting("token", cfg_.jwt_secret) + "-share-access";
        if (tok.empty() || !verify_url_payload(secret, share_token_payload(*sh), tok)) {
            resp.set_json(401, R"({"code":401,"message":"valid share token required","data":null})");
            return;
        }
    }

    std::string target_vpath = sh->path;
    if (sh->is_dir) {
        auto clean_sub = clean_vpath(sub_sv);
        if (!clean_sub || *clean_sub == "/") {
            resp.set_json(400, R"({"code":400,"message":"file subpath required for folder share","data":null})");
            return;
        }
        auto joined = join_vpath(sh->path, *clean_sub);
        if (!joined || !is_subpath(sh->path, *joined)) {
            resp.set_json(403, R"({"code":403,"message":"path outside shared root","data":null})");
            return;
        }
        target_vpath = std::move(*joined);
    }
    // A share is not a permanent bypass for path-meta hide rules. Re-check both
    // the original file share and every descendant on each public request.
    if (vfs_.is_hidden_by_meta(target_vpath)) {
        resp.set_json(404, R"({"code":404,"message":"shared file not found","data":null})");
        return;
    }

    std::string derr;
    auto dl = vfs_.resolve_download(target_vpath, req.headers, derr);
    if (!dl) {
        resp.set_json(404, R"({"code":404,"message":"shared file unavailable","data":null})");
        return;
    }
    // Count each successful GET request, including byte ranges. Treating only
    // `bytes=0-` as a download lets a caller evade max_downloads with `bytes=1-`.
    if (req.method == "GET" && !db_.increment_share_counter(share_id, true)) {
        resp.set_json(403, R"({"code":403,"message":"share download quota exhausted","data":null})");
        return;
    }
    WebDavServer::serve_resolution(req, resp, *dl, force_proxy, true);
}

// Experimental stateless MCP 2025-03-26 over HTTP, deliberately limited to
// read-only metadata. It does not expose raw URLs, contents or mutations.
void ApiRouter::handle_mcp(HttpRequest& req, HttpResponse& resp) {
    resp.headers["Cache-Control"] = "no-store";
    resp.headers["X-Content-Type-Options"] = "nosniff";
    if (!cfg_.enable_mcp) { resp.status = 404; return; }
    // Do not expose bearer-authenticated MCP to browser origins. Non-browser
    // clients must omit Origin; deploy behind TLS and a trusted proxy.
    if (!req.header("origin").empty()) { resp.status = 403; return; }
    if (req.method != "POST") { resp.status = 405; resp.headers["Allow"] = "POST"; return; }
    if (!to_lower_ascii(req.header("content-type")).starts_with("application/json")) { resp.status = 415; return; }
    if (req.body.size() > 65536) { resp.status = 413; return; }
    auto auth = authenticate(req, resp, false);
    if (!auth) { resp.status = 401; return; }
    const auto& user = auth->user;
    if (user.is_guest()) { resp.status = 403; return; }

    auto send_error = [&](const JsonValue& id, int code, std::string_view message) {
        resp.set_json(200, JsonValue::object({{"jsonrpc", "2.0"}, {"id", id},
            {"error", JsonValue::object({{"code", code}, {"message", std::string(message)}})}}).dump());
    };
    auto parsed = JsonValue::parse(req.body);
    if (!parsed) { send_error(nullptr, -32700, "Parse error"); return; }
    const auto& j = *parsed;
    if (!j.is_object() || j.get("jsonrpc").as_sv() != "2.0" || !j.get("method").is_string()) {
        send_error(nullptr, -32600, "Invalid request"); return;
    }
    bool has_id = j.contains("id");
    const auto& id = j.get("id");
    if (has_id && !(id.is_string() || id.is_int())) { send_error(nullptr, -32600, "Invalid request id"); return; }
    if (!has_id) { resp.status = 202; return; }
    auto send_result = [&](JsonValue result) {
        resp.set_json(200, JsonValue::object({{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}}).dump());
    };
    auto tool_result = [&](const JsonValue& value, bool error = false) {
        return JsonValue::object({{"content", JsonValue::array({JsonValue::object({{"type", "text"}, {"text", value.dump()}})})}, {"isError", error}});
    };
    const std::string method = j.get("method").as_str();
    if (method == "initialize") {
        send_result(JsonValue::object({{"protocolVersion", "2025-03-26"},
            {"capabilities", JsonValue::object({{"tools", JsonValue::object({{"listChanged", false}})}})},
            {"serverInfo", JsonValue::object({{"name", "kova-readonly"}, {"version", kVersion}})}}));
        return;
    }
    if (method == "ping") { send_result(JsonValue::object()); return; }
    if (method == "tools/list") {
        auto schema = JsonValue::object({{"type", "object"},
            {"properties", JsonValue::object({{"path", JsonValue::object({{"type", "string"}, {"description", "Absolute path within the user base"}})},
                {"page", JsonValue::object({{"type", "integer"}, {"minimum", 1}})}})},
            {"required", JsonValue::array({"path"})}, {"additionalProperties", false}});
        send_result(JsonValue::object({{"tools", JsonValue::array({
            JsonValue::object({{"name", "fs_list"}, {"description", "List visible entries in a directory, up to 50 per page"}, {"inputSchema", schema}}),
            JsonValue::object({{"name", "fs_get"}, {"description", "Read metadata of one visible file or directory"}, {"inputSchema", schema}})
        })}}));
        return;
    }
    if (method != "tools/call") { send_error(id, -32601, "Method not found"); return; }
    const auto& params = j.get("params");
    const auto& args = params.get("arguments");
    const std::string name = params.get("name").as_str();
    if (!params.is_object() || !args.is_object() || (name != "fs_list" && name != "fs_get") ||
        !args.get("path").is_string() || args.get("path").as_sv().empty() ||
        args.get("path").as_sv().front() != '/' || args.get("path").size() > 4096 ||
        (args.contains("page") && (!args.get("page").is_int() || args.get("page").as_i64() < 1 || args.get("page").as_i64() > 1000000))) {
        send_error(id, -32602, "Invalid tool arguments"); return;
    }
    for (const auto& [key, _] : args.as_object()) {
        if (key != "path" && key != "page") { send_error(id, -32602, "Invalid tool arguments"); return; }
    }
    auto path = vfs_.resolve_user_path(user, args.get("path").as_sv());
    // MCP deliberately treats any inherited password as a barrier, even if a
    // closer metadata record has no password. Snapshot once per request so
    // checking every listed child does not repeatedly copy the metadata cache.
    const auto metas = db_.list_metas();
    auto protected_by_password = [&](std::string_view target) {
        for (const auto& meta : metas) {
            if (!meta.password.empty() &&
                (meta.path == target || (meta.p_sub && is_subpath(meta.path, target)))) return true;
        }
        return false;
    };
    if (!path || !has_perm(vfs_.effective_permissions(user, *path), PermMCPAccess) || protected_by_password(*path)) {
        send_result(tool_result(JsonValue::object({{"error", "MCP access denied"}}), true)); return;
    }
    auto metadata = [](const FileObj& entry) {
        return JsonValue::object({{"name", entry.name}, {"size", entry.size},
            {"is_dir", entry.is_dir}, {"modified", entry.format_iso8601_str()}, {"type", entry.type}});
    };
    if (name == "fs_list") {
        auto listing = vfs_.list_dir(&user, *path, "", false);
        if (!listing.ok) { send_result(tool_result(JsonValue::object({{"error", "Directory unavailable or access denied"}}), true)); return; }
        JsonValue content = JsonValue::array();
        size_t page = static_cast<size_t>(args.get("page").as_i64(1));
        size_t start = (page - 1) * size_t{50};
        size_t total = 0;
        for (const auto& entry : listing.entries) {
            auto child = join_vpath(*path, entry.name);
            if (!child || protected_by_password(*child) || !has_perm(vfs_.effective_permissions(user, *child), PermMCPAccess) ||
                !vfs_.can_read_path(user, *child, "")) continue;
            if (total >= start && total - start < 50) content.push_back(metadata(entry));
            ++total;
        }
        send_result(tool_result(JsonValue::object({{"content", std::move(content)},
            {"page", static_cast<int64_t>(page)}, {"total", static_cast<int64_t>(total)}})));
        return;
    }
    auto obj = vfs_.get_obj(&user, *path, "", "");
    if (!obj.ok) { send_result(tool_result(JsonValue::object({{"error", "Object unavailable or access denied"}}), true)); return; }
    send_result(tool_result(metadata(obj.entry)));
}

void ApiRouter::dispatch(HttpRequest& req, HttpResponse& resp) {
    if (req.path == "/api/mcp") { handle_mcp(req, resp); return; }
    // Global CORS headers (supports live preview hosts and cross-origin WebDAV/API clients)
    std::string origin = req.header("origin");
    if (origin.empty()) {
        resp.headers["Access-Control-Allow-Origin"] = "*";
    } else {
        resp.headers["Access-Control-Allow-Origin"] = origin;
        resp.headers["Vary"] = "Origin";
    }
    resp.headers["Access-Control-Allow-Methods"] =
        "GET, POST, PUT, DELETE, PATCH, OPTIONS, HEAD, PROPFIND, PROPPATCH, MKCOL, COPY, MOVE, LOCK, UNLOCK";
    resp.headers["Access-Control-Allow-Headers"] =
        "Origin, Content-Length, Content-Type, Authorization, Password, File-Path, As-Task, Overwrite, "
        "Range, If-None-Match, If-Modified-Since, Depth, Destination, Lock-Token, Timeout, If, Client-Id, X-Share-Token, X-OTP-Code";
    resp.headers["Access-Control-Expose-Headers"] = "Content-Length, Content-Range, Content-Disposition, ETag";

    auto effective_base_url = [&]() -> std::string {
        return cfg_.site_url.empty() ? req.base_url() : cfg_.site_url;
    };

    if (req.method == "OPTIONS" && !req.path.starts_with("/dav")) {
        resp.status = 204;
        return;
    }

    // 1. Fast Health & Robots endpoints
    if (req.path == "/ping") {
        resp.status = 200;
        resp.headers["Content-Type"] = "text/plain; charset=utf-8";
        resp.body = "pong";
        return;
    }
    if (req.path == "/robots.txt") {
        resp.status = 200;
        resp.headers["Content-Type"] = "text/plain; charset=utf-8";
        resp.body = db_.get_setting("robots_txt", "User-agent: *\nDisallow: /");
        return;
    }

    // 2. RFC 4918 WebDAV Gateway (/dav & /dav/*)
    if (req.path == "/dav" || req.path.starts_with("/dav/")) {
        webdav_.handle(req, resp);
        return;
    }

    // 3. Direct / Proxy / Archive / Share Data Plane
    if (req.path.starts_with("/d/"))  { handle_direct_or_proxy(req, resp, false); return; }
    if (req.path.starts_with("/p/"))  { handle_direct_or_proxy(req, resp, true);  return; }
    if (req.path.starts_with("/ad/")) { handle_archive_extract(req, resp);        return; }
    if (req.path.starts_with("/sd/")) { handle_share_download(req, resp, false);  return; }
    if (req.path.starts_with("/sp/")) { handle_share_download(req, resp, true);   return; }
    if (req.path.starts_with("/s/"))  { serve_spa(req, resp);                     return; }

    // 4. Non-API routes -> Embedded Single-Page Application
    if (!req.path.starts_with("/api/")) {
        serve_spa(req, resp);
        return;
    }

    // ========================================================================
    // 5. Public Unauthenticated API Endpoints (/api/public/* & /api/auth/login)
    // ========================================================================
    if (req.path == "/api/public/settings") {
        auto settings = db_.list_settings(true);
        JsonValue data = JsonValue::object();
        for (const auto& [k, v] : settings) data[k] = v;
        data["app_name"] = kAppName;
        data["version"] = kVersion;
        data["web_version"] = kWebVersion;
        data["engine"] = kEngine;
        data["author"] = kAuthor;
        data["author_url"] = kAuthorUrl;
        auto guest = db_.get_guest_user();
        data["guest_enabled"] = (guest && !guest->disabled);
        json_ok(resp, std::move(data));
        return;
    }
    if (req.path == "/api/public/offline_download_tools") {
        json_ok(resp, JsonValue::array({"SimpleHttp"}));
        return;
    }
    if (req.path == "/api/public/archive_extensions") {
        json_ok(resp, JsonValue::array({"zip"}));
        return;
    }
    if (req.path == "/api/public/share/info") {
        std::string sid = req.query_param("share_id");
        auto sh = db_.get_share(sid);
        if (!sh || sh->disabled || (sh->expires_at > 0 && unix_now() > sh->expires_at) ||
            vfs_.is_hidden_by_meta(sh->path)) {
            json_err(resp, 404, "Share link does not exist or has expired");
            return;
        }
        db_.increment_share_counter(sid, false);
        auto [_, base] = split_vpath(sh->path);
        std::string display_name = base.empty() ? "/" : base;
        int64_t fsize = 0;
        int ftype = sh->is_dir ? 1 : classify_file_type(display_name, false);
        int64_t fmod = sh->updated_at;
        if (!sh->is_dir && !sh->has_password) {
            auto obj = vfs_.get_obj(nullptr, sh->path, "", "");
            if (obj.ok) {
                fsize = obj.entry.size;
                ftype = obj.entry.type;
                fmod  = obj.entry.modified;
            }
        }
        json_ok(resp, JsonValue::object({
            {"share_id",      sh->share_id},
            {"name",          display_name},
            {"is_dir",        sh->is_dir},
            {"size",          fsize},
            {"type",          ftype},
            {"modified",      fmod},
            {"creator",       sh->creator_name},
            {"has_password",  sh->has_password},
            {"expires_at",    sh->expires_at},
            {"max_downloads", sh->max_downloads},
            {"downloads",     sh->downloads},
            {"views",         sh->views + 1},
        }));
        return;
    }
    if (req.path == "/api/public/share/auth" && req.method == "POST") {
        if (share_bf_.is_blocked(req.remote_ip)) {
            json_err(resp, 429, "Too many wrong share passwords, please wait 5 minutes");
            return;
        }
        auto body = parse_body_or_empty(req);
        std::string sid = body.get("share_id").as_str();
        std::string pw  = body.get("password").as_str();
        auto sh = db_.get_share(sid);
        if (!sh || sh->disabled || (sh->expires_at > 0 && unix_now() > sh->expires_at) ||
            vfs_.is_hidden_by_meta(sh->path)) {
            json_err(resp, 404, "Share not found");
            return;
        }
        if (sh->has_password && !verify_password_pbkdf2(pw, sh->password_hash)) {
            share_bf_.record_failure(req.remote_ip);
            json_err(resp, 403, "Incorrect share password");
            return;
        }
        share_bf_.record_success(req.remote_ip);
        std::string secret = db_.get_setting("token", cfg_.jwt_secret) + "-share-access";
        std::string tok = sign_url_payload(secret, share_token_payload(*sh), unix_now() + 86400);
        json_ok(resp, JsonValue::object({{"token", tok}}));
        return;
    }
    if (req.path == "/api/public/share/list" && req.method == "POST") {
        auto body = parse_body_or_empty(req);
        std::string sid = body.get("share_id").as_str();
        std::string sub = body.get("path").as_str("/");
        auto sh = db_.get_share(sid);
        if (!sh || sh->disabled || !sh->is_dir || (sh->expires_at > 0 && unix_now() > sh->expires_at) ||
            vfs_.is_hidden_by_meta(sh->path)) {
            json_err(resp, 404, "Folder share not found or expired");
            return;
        }
        if (sh->has_password) {
            std::string tok = req.header("x-share-token", body.get("token").as_str());
            std::string secret = db_.get_setting("token", cfg_.jwt_secret) + "-share-access";
            if (tok.empty() || !verify_url_payload(secret, share_token_payload(*sh), tok)) {
                json_err(resp, 401, "Share authentication required");
                return;
            }
        }
        auto clean_sub = clean_vpath(sub);
        auto target = clean_sub ? join_vpath(sh->path, *clean_sub) : std::nullopt;
        if (!target || !is_subpath(sh->path, *target)) {
            json_err(resp, 403, "Illegal share subpath");
            return;
        }
        if (vfs_.is_hidden_by_meta(*target)) {
            json_err(resp, 404, "Folder not found");
            return;
        }
        auto ls = vfs_.list_dir(nullptr, *target, "", false);
        if (!ls.ok) { json_err(resp, ls.http_status, ls.error); return; }
        JsonValue arr = JsonValue::array();
        for (const auto& f : ls.entries) {
            auto child_vp = join_vpath(*target, f.name);
            if (!child_vp || !is_subpath(*target, *child_vp) || vfs_.is_hidden_by_meta(*child_vp)) continue;
            arr.push_back(f.to_json());
        }
        int64_t visible_total = static_cast<int64_t>(arr.as_array().size());
        json_ok(resp, JsonValue::object({
            {"content", std::move(arr)},
            {"total",   visible_total},
        }));
        return;
    }

    // Login Endpoint (constant-effort PBKDF2 + per-IP Brute-Force + TOTP 2FA Replay Protection)
    if (req.path == "/api/auth/login" && req.method == "POST") {
        if (login_bf_.is_blocked(req.remote_ip)) {
            json_err(resp, 429, "Too many failed login attempts. Locked out for 5 minutes.");
            return;
        }
        auto body = parse_body_or_empty(req);
        std::string uname = body.get("username").as_str();
        std::string pwd   = body.get("password").as_str();
        std::string otp   = body.get("otp_code").as_str();

        auto u = db_.get_user_by_name(uname);
        bool account_valid = (u && !u->is_guest() && !u->disabled && !u->password_hash.empty());
        bool pwd_ok = verify_password_pbkdf2(pwd, account_valid ? u->password_hash : dummy_pwd_hash_);
        if (!account_valid || !pwd_ok) {
            login_bf_.record_failure(req.remote_ip);
            json_err(resp, 400, "Invalid username or password");
            return;
        }

        if (!u->otp_secret.empty()) {
            int64_t matched_step = -1;
            if (!totp_verify(u->otp_secret, otp, unix_now(), &matched_step)) {
                login_bf_.record_failure(req.remote_ip);
                json_err(resp, 402, "Invalid or missing 2FA OTP code");
                return;
            }
            if (!db_.verify_and_advance_otp_step(u->id, matched_step)) {
                json_err(resp, 402, "OTP code already used in this time window (replay blocked)");
                return;
            }
        }

        std::string cid = req.header("client-id", "default");
        std::string dev_key = sha256_hex(std::to_string(u->id) + ":" + cid).substr(0, 32);
        std::string serr;
        if (!db_.check_or_touch_session(u->id, dev_key, req.header("user-agent"), req.remote_ip, true, &serr)) {
            json_err(resp, 403, serr);
            return;
        }

        login_bf_.record_success(req.remote_ip);
        int64_t now = unix_now();
        JwtClaims claims;
        claims.username = u->username;
        claims.user_id  = static_cast<uint64_t>(u->id);
        claims.pwd_ts   = u->pwd_ts;
        claims.iat      = now;
        claims.nbf      = now;
        claims.exp      = now + static_cast<int64_t>(cfg_.token_expires_in) * 3600;
        claims.jti      = random_hex(12);

        std::string jwt = jwt_encode_hs256(claims, cfg_.jwt_secret);
        json_ok(resp, JsonValue::object({{"token", jwt}}));
        return;
    }

    // ========================================================================
    // 6. Authenticated Endpoints (Guest allowed on read endpoints if enabled)
    // ========================================================================
    bool guest_ok = (req.path == "/api/me" || req.path.starts_with("/api/fs/"));
    auto auth = authenticate(req, resp, guest_ok);
    if (!auth) return;
    const UserRecord& user = auth->user;

    if (req.path == "/api/auth/logout") {
        if (!auth->jti.empty()) {
            db_.revoke_token_jti(auth->jti, auth->token_exp);
        }
        db_.evict_session(user.id, auth->device_key);
        json_ok(resp);
        return;
    }

    if (req.path == "/api/me") {
        int32_t eff_perm = vfs_.effective_permissions(user, user.base_path);
        JsonValue rids = JsonValue::array();
        for (int64_t id : user.role_ids) rids.push_back(id);
        json_ok(resp, JsonValue::object({
            {"id",         user.id},
            {"username",   user.username},
            {"base_path",  user.base_path},
            {"role",       user.identity},
            {"role_ids",   std::move(rids)},
            {"permission", eff_perm},
            {"disabled",   user.disabled},
            {"otp",        !user.otp_secret.empty()},
        }));
        return;
    }

    if (req.path == "/api/me/update" && req.method == "POST") {
        if (user.is_guest()) { json_err(resp, 403, "Guest cannot update profile"); return; }
        auto body = parse_body_or_empty(req);
        std::string new_pwd = body.get("password").as_str();
        if (!new_pwd.empty()) {
            if (new_pwd.size() < 6) { json_err(resp, 400, "Password must be at least 6 characters"); return; }
            db_.update_user_password(user.id, new_pwd);
        }
        json_ok(resp);
        return;
    }

    if (req.path == "/api/auth/2fa/generate" && req.method == "POST") {
        if (user.is_guest()) { json_err(resp, 403, "Forbidden"); return; }
        std::string sec = totp_generate_secret();
        std::string uri = "otpauth://totp/Kova:" + url_encode(user.username) +
                          "?secret=" + sec + "&issuer=Kova";
        json_ok(resp, JsonValue::object({{"secret", sec}, {"qr", uri}}));
        return;
    }

    if (req.path == "/api/auth/2fa/cancel" && req.method == "POST") {
        if (user.is_guest()) { json_err(resp, 403, "Forbidden"); return; }
        auto body = parse_body_or_empty(req);
        std::string code = body.get("code").as_str();
        int64_t step = -1;
        if (!user.otp_secret.empty() &&
            (!totp_verify(user.otp_secret, code, unix_now(), &step) ||
             !db_.verify_and_advance_otp_step(user.id, step))) {
            json_err(resp, 400, "Valid 6-digit 2FA code required to disable 2FA");
            return;
        }
        db_.update_user_otp(user.id, "", -1);
        json_ok(resp);
        return;
    }

    if (req.path == "/api/auth/2fa/verify" && req.method == "POST") {
        if (user.is_guest()) { json_err(resp, 403, "Forbidden"); return; }
        auto body = parse_body_or_empty(req);
        std::string sec  = body.get("secret").as_str();
        std::string code = body.get("code").as_str();
        int64_t step = -1;
        if (sec.empty() || !totp_verify(sec, code, unix_now(), &step)) {
            json_err(resp, 400, "Invalid 2FA verification code");
            return;
        }
        db_.update_user_otp(user.id, sec, step);
        json_ok(resp);
        return;
    }

    if (req.path == "/api/me/sessions") {
        auto list = db_.list_sessions(user.id);
        JsonValue arr = JsonValue::array();
        for (const auto& s : list) {
            arr.push_back(JsonValue::object({
                {"device_key",  s.device_key},
                {"user_agent",  s.user_agent},
                {"ip",          s.ip},
                {"last_active", s.last_active},
                {"status",      s.status},
                {"current",     s.device_key == auth->device_key},
            }));
        }
        json_ok(resp, std::move(arr));
        return;
    }

    if (req.path == "/api/me/sessions/evict" && req.method == "POST") {
        auto body = parse_body_or_empty(req);
        db_.evict_session(user.id, body.get("device_key").as_str());
        json_ok(resp);
        return;
    }

    // ========================================================================
    // 7. Virtual Filesystem API (/api/fs/*)
    // ========================================================================
    if (req.path == "/api/fs/list" || req.path == "/api/fs/dirs") {
        auto body = parse_body_or_empty(req);
        std::string req_p = body.get("path").as_str(req.query_param("path", "/"));
        std::string pw    = body.get("password").as_str(req.header("password"));
        bool refresh      = body.get("refresh").as_bool(false);
        int page          = bounded_json_int(body.get("page"), 1, 1, std::numeric_limits<int>::max());
        int per_page      = bounded_json_int(body.get("per_page"), 0, 0, 500);

        auto gvpath = vfs_.resolve_user_path(user, req_p);
        if (!gvpath) { json_err(resp, 403, "Invalid path"); return; }

        auto ls = vfs_.list_dir(&user, *gvpath, pw, refresh);
        if (!ls.ok) { json_err(resp, ls.http_status, ls.error); return; }

        bool dirs_only = (req.path == "/api/fs/dirs");
        std::vector<const FileObj*> filtered;
        for (const auto& e : ls.entries) {
            if (dirs_only && !e.is_dir) continue;
            filtered.push_back(&e);
        }

        int64_t total = static_cast<int64_t>(filtered.size());
        size_t start = 0, end = filtered.size();
        if (per_page > 0) {
            const size_t page_index = static_cast<size_t>(page - 1);
            const size_t page_size = static_cast<size_t>(per_page);
            start = (page_index > filtered.size() / page_size)
                        ? filtered.size()
                        : std::min(filtered.size(), page_index * page_size);
            end = start + std::min(page_size, filtered.size() - start);
        }
        JsonValue arr = JsonValue::array();
        for (size_t i = start; i < end; ++i) arr.push_back(filtered[i]->to_json());

        if (dirs_only) {
            json_ok(resp, std::move(arr));
        } else {
            json_ok(resp, JsonValue::object({
                {"content",  std::move(arr)},
                {"total",    total},
                {"readme",   ls.readme},
                {"header",   ls.header},
                {"write",    ls.write_allowed},
                {"provider", ls.provider},
            }));
        }
        return;
    }

    if (req.path == "/api/fs/get") {
        auto body = parse_body_or_empty(req);
        std::string req_p = body.get("path").as_str(req.query_param("path", "/"));
        std::string pw    = body.get("password").as_str(req.header("password"));
        auto gvpath = vfs_.resolve_user_path(user, req_p);
        if (!gvpath) { json_err(resp, 403, "Invalid path"); return; }

        auto gr = vfs_.get_obj(&user, *gvpath, pw, effective_base_url());
        if (!gr.ok) { json_err(resp, gr.http_status, gr.error); return; }

        JsonValue d = gr.entry.to_json();
        d["raw_url"]  = gr.raw_url;
        d["readme"]   = gr.readme;
        d["header"]   = gr.header;
        d["provider"] = gr.provider;
        json_ok(resp, std::move(d));
        return;
    }

    if (req.path == "/api/fs/search") {
        auto body = parse_body_or_empty(req);
        std::string parent_req = body.get("parent").as_str("/");
        std::string keywords   = body.get("keywords").as_str();
        int scope              = bounded_json_int(body.get("scope"), 0, 0, 2);
        int page               = bounded_json_int(body.get("page"), 1, 1, std::numeric_limits<int>::max());
        int per_page           = bounded_json_int(body.get("per_page"), 50, 1, 500);

        auto gparent = vfs_.resolve_user_path(user, parent_req);
        if (!gparent || !vfs_.can_read_path(user, *gparent, body.get("password").as_str())) {
            json_err(resp, 403, "Permission denied");
            return;
        }

        auto [hits, total] = db_.search_files(keywords, *gparent, scope, page, per_page);
        JsonValue arr = JsonValue::array();
        for (const auto& h : hits) {
            auto full_h = join_vpath(h.parent, h.name).value_or(h.parent);
            if (!vfs_.can_read_path(user, full_h, "")) continue; // Enforce RBAC & Hide rules on search results!
            std::string rel_parent = relative_subpath(user.base_path, h.parent);
            arr.push_back(JsonValue::object({
                {"parent", rel_parent},
                {"name",   h.name},
                {"is_dir", h.is_dir},
                {"size",   h.size},
                {"type",   h.type},
            }));
        }
        json_ok(resp, JsonValue::object({
            {"content", std::move(arr)},
            {"total",   total},
        }));
        return;
    }

    if (req.path == "/api/fs/mkdir" && req.method == "POST") {
        auto body = parse_body_or_empty(req);
        auto gv = vfs_.resolve_user_path(user, body.get("path").as_str());
        if (!gv) { json_err(resp, 400, "Illegal path"); return; }
        auto [par, name] = split_vpath(*gv);
        if (!is_valid_filename(name)) { json_err(resp, 400, "Invalid folder name"); return; }
        if (!vfs_.can_write_path(user, par, PermWrite, req.header("password"))) {
            json_err(resp, 403, "Write permission denied");
            return;
        }
        std::string err;
        if (!vfs_.mkdir(*gv, err)) { json_err(resp, 500, err); return; }
        json_ok(resp);
        return;
    }

    if (req.path == "/api/fs/rename" && req.method == "POST") {
        auto body = parse_body_or_empty(req);
        auto gv = vfs_.resolve_user_path(user, body.get("path").as_str());
        std::string new_name = body.get("name").as_str();
        if (!gv || !is_valid_filename(new_name)) { json_err(resp, 400, "Invalid path or name"); return; }
        if (!vfs_.can_write_path(user, *gv, PermRename, req.header("password"))) {
            json_err(resp, 403, "Rename permission denied");
            return;
        }
        std::string err;
        if (!vfs_.rename_obj(*gv, new_name, err)) { json_err(resp, 500, err); return; }
        json_ok(resp);
        return;
    }

    if (req.path == "/api/fs/remove" && req.method == "POST") {
        auto body = parse_body_or_empty(req);
        auto gdir = vfs_.resolve_user_path(user, body.get("dir").as_str());
        if (!gdir) { json_err(resp, 400, "Invalid dir"); return; }
        const auto& names = body.get("names").as_array();
        if (names.size() > 500) { json_err(resp, 400, "A maximum of 500 items can be removed per request"); return; }
        std::string pw = body.get("password").as_str(req.header("password"));
        std::string err;
        for (const auto& nv : names) {
            std::string nm = nv.as_str();
            if (!is_valid_filename(nm)) continue;
            auto target = join_vpath(*gdir, nm);
            if (!target) continue;
            if (!vfs_.can_write_path(user, *target, PermRemove, pw)) {
                json_err(resp, 403, "Remove permission denied on " + nm);
                return;
            }
            if (!vfs_.remove(*target, err)) { json_err(resp, 500, err); return; }
        }
        json_ok(resp);
        return;
    }

    if ((req.path == "/api/fs/move" || req.path == "/api/fs/copy") && req.method == "POST") {
        auto body = parse_body_or_empty(req);
        auto src_dir = vfs_.resolve_user_path(user, body.get("src_dir").as_str());
        auto dst_dir = vfs_.resolve_user_path(user, body.get("dst_dir").as_str());
        if (!src_dir || !dst_dir) { json_err(resp, 400, "Invalid source or destination"); return; }

        const auto& names = body.get("names").as_array();
        if (names.size() > 100) { json_err(resp, 400, "A maximum of 100 items can be moved or copied per request"); return; }

        std::string src_pw = body.get("password").as_str(req.header("password"));
        std::string dst_pw = body.get("dst_password").as_str(src_pw);
        bool is_move = (req.path == "/api/fs/move");
        PermissionBit bit = is_move ? PermMove : PermCopy;
        if (!vfs_.can_write_path(user, *dst_dir, bit, dst_pw) || !vfs_.is_path_in_role_scope(user, *dst_dir)) {
            json_err(resp, 403, "Destination permission denied");
            return;
        }

        std::string err;
        JsonValue task_ids = JsonValue::array();
        for (const auto& nv : names) {
            std::string nm = nv.as_str();
            if (!is_valid_filename(nm)) continue;
            auto src_full = join_vpath(*src_dir, nm);
            if (!src_full) continue;
            if (is_move) {
                if (!vfs_.can_write_path(user, *src_full, PermMove, src_pw)) {
                    json_err(resp, 403, "Move permission denied");
                    return;
                }
                if (!vfs_.move_obj(*src_full, *dst_dir, err)) { json_err(resp, 500, err); return; }
            } else {
                if (!vfs_.can_read_path(user, *src_full, src_pw)) {
                    json_err(resp, 403, "Read permission denied on source");
                    return;
                }
                bool async_task = body.get("as_task").as_bool(false);
                if (async_task) {
                    auto task_id = tasks_.enqueue_copy(user.username, *src_full, *dst_dir);
                    if (task_id.empty()) { json_err(resp, 429, "Task queue is full; some earlier items may already have been queued"); return; }
                    task_ids.push_back(std::move(task_id));
                } else {
                    if (!vfs_.copy_obj_sync(*src_full, *dst_dir, err)) { json_err(resp, 500, err); return; }
                }
            }
        }
        json_ok(resp, JsonValue::object({{"tasks", std::move(task_ids)}}));
        return;
    }

    if (req.path == "/api/fs/put" && req.method == "PUT") {
        std::string raw_fp = req.header("file-path");
        auto dec_fp = url_decode(raw_fp);
        if (!dec_fp || dec_fp->empty()) { json_err(resp, 400, "Missing File-Path header"); return; }
        auto gv = vfs_.resolve_user_path(user, *dec_fp);
        if (!gv) { json_err(resp, 400, "Illegal File-Path"); return; }
        auto [par, fname] = split_vpath(*gv);
        if (!is_valid_filename(fname)) { json_err(resp, 400, "Invalid filename"); return; }
        if (!vfs_.can_write_path(user, par, PermWrite, req.header("password"))) {
            json_err(resp, 403, "Upload permission denied");
            return;
        }
        const int64_t upload_limit = std::max<int64_t>(0, cfg_.max_upload_bytes);
        if (!req.is_chunked && req.content_length > upload_limit) {
            json_err(resp, 413, "File exceeds max_upload_bytes limit", 413);
            return;
        }

        UploadStream up;
        up.filename = fname;
        up.size = req.is_chunked ? -1 : req.content_length;
        up.mime_type = req.header("content-type", std::string(guess_mime_type(fname)));
        int64_t total_read = 0;
        up.read_fn = [&](char* buf, size_t max_len) -> ssize_t {
            if (!req.stream_body) return 0;
            ssize_t n = req.stream_body(buf, max_len);
            if (n > 0) {
                if (total_read > upload_limit || n > upload_limit - total_read) return -1;
                total_read += n;
                up_limiter_.consume(static_cast<size_t>(n));
            }
            return n;
        };
        std::string err;
        if (!vfs_.put_stream(*gv, up, err)) {
            json_err(resp, 500, err);
            return;
        }
        json_ok(resp);
        return;
    }

    if (req.path == "/api/fs/add_offline_download" && req.method == "POST") {
        auto body = parse_body_or_empty(req);
        std::string pw = body.get("password").as_str(req.header("password"));
        auto gdir = vfs_.resolve_user_path(user, body.get("path").as_str("/"));
        if (!gdir || !vfs_.can_write_path(user, *gdir, PermAddOfflineDownload, pw)) {
            json_err(resp, 403, "Offline download permission denied");
            return;
        }
        const auto& urls = body.get("urls").as_array();
        if (urls.size() > 100) { json_err(resp, 400, "A maximum of 100 offline downloads can be queued per request"); return; }
        JsonValue tids = JsonValue::array();
        for (const auto& uv : urls) {
            std::string u = uv.as_str();
            std::string verr;
            if (!validate_outbound_url(u, cfg_.allow_private_net_outbound, &verr)) {
                json_err(resp, 400, verr);
                return;
            }
            auto task_id = tasks_.enqueue_offline_download(user.username, std::move(u), *gdir);
            if (task_id.empty()) { json_err(resp, 429, "Task queue is full; some earlier URLs may already have been queued"); return; }
            tids.push_back(std::move(task_id));
        }
        json_ok(resp, JsonValue::object({{"tasks", std::move(tids)}}));
        return;
    }

    if (req.path == "/api/fs/archive/list" && req.method == "POST") {
        auto body = parse_body_or_empty(req);
        auto gv = vfs_.resolve_user_path(user, body.get("path").as_str());
        if (!gv) { json_err(resp, 400, "Illegal path"); return; }
        int32_t perms = vfs_.effective_permissions(user, *gv);
        if (!user.is_admin() && !has_perm(perms, PermReadArchives)) {
            json_err(resp, 403, "Archive read permission denied");
            return;
        }
        if (!vfs_.can_read_path(user, *gv, body.get("password").as_str())) {
            json_err(resp, 403, "Access denied");
            return;
        }
        std::string derr;
        auto dl = vfs_.resolve_download(*gv, req.headers, derr);
        if (!dl || dl->mode != DownloadResolution::Mode::LocalFile) {
            json_err(resp, 400, "Archive listing requires local file");
            return;
        }
        std::vector<ZipEntryInfo> entries;
        if (!zip_list_entries(dl->local_path, entries, derr)) {
            json_err(resp, 400, derr);
            return;
        }
        std::string sys_tok = db_.get_setting("token", cfg_.jwt_secret);
        JsonValue arr = JsonValue::array();
        for (const auto& e : entries) {
            std::string sig = sign_url_payload(sys_tok, *gv + "\x00" + e.path, unix_now() + 7200);
            arr.push_back(JsonValue::object({
                {"path",   e.path},
                {"is_dir", e.is_dir},
                {"size",   static_cast<int64_t>(e.uncompressed_size)},
                {"sign",   sig},
            }));
        }
        json_ok(resp, JsonValue::object({{"content", std::move(arr)}}));
        return;
    }

    if (req.path == "/api/fs/archive/decompress" && req.method == "POST") {
        auto body = parse_body_or_empty(req);
        std::string src_pw = body.get("password").as_str(req.header("password"));
        std::string dst_pw = body.get("dst_password").as_str(src_pw);
        auto src_v = vfs_.resolve_user_path(user, body.get("src_path").as_str(body.get("path").as_str()));
        auto dst_v = vfs_.resolve_user_path(user, body.get("dst_dir").as_str());
        if (!src_v || !dst_v) { json_err(resp, 400, "Invalid paths"); return; }
        if (!vfs_.can_write_path(user, *dst_v, PermDecompress, dst_pw) || !vfs_.can_read_path(user, *src_v, src_pw)) {
            json_err(resp, 403, "Decompress permission denied");
            return;
        }
        std::string err;
        auto src_dl = vfs_.resolve_download(*src_v, req.headers, err);
        if (!src_dl || src_dl->mode != DownloadResolution::Mode::LocalFile) {
            json_err(resp, 400, "Source archive must be on local storage");
            return;
        }
        // Extract into temporary directory then move/put into target VFS!
        std::string tmp_extract = cfg_.temp_dir + "/unzip_" + random_hex(8);
        ScopeExit clean_tmp([&] {
            std::error_code ec;
            std::filesystem::remove_all(tmp_extract, ec);
        });
        if (!zip_decompress_to_dir(src_dl->local_path, tmp_extract, err)) {
            json_err(resp, 400, err);
            return;
        }
        // Upload extracted tree into VFS destination
        for (const auto& dir_ent : std::filesystem::recursive_directory_iterator(tmp_extract)) {
            auto rel = std::filesystem::relative(dir_ent.path(), tmp_extract).generic_string();
            auto target_vp = join_vpath(*dst_v, rel);
            if (!target_vp) continue;
            if (dir_ent.is_directory()) {
                vfs_.mkdir(*target_vp, err);
            } else if (dir_ent.is_regular_file()) {
                std::string fp = dir_ent.path().string();
                UniqueFd rfd(::open(fp.c_str(), O_RDONLY | O_CLOEXEC));
                if (!rfd) continue;
                UploadStream up;
                up.filename = dir_ent.path().filename().string();
                up.size = static_cast<int64_t>(dir_ent.file_size());
                up.read_fn = [&](char* b, size_t m) -> ssize_t { return ::read(rfd.get(), b, m); };
                if (!vfs_.put_stream(*target_vp, up, err)) { json_err(resp, 500, err); return; }
            }
        }
        json_ok(resp);
        return;
    }

    // ========================================================================
    // 8. Share & Task Management (Non-Guest Users)
    // ========================================================================
    if (req.path.starts_with("/api/share/") || req.path.starts_with("/api/task/")) {
        if (user.is_guest()) { json_err(resp, 403, "Guest cannot manage shares or tasks"); return; }

        if (req.path == "/api/share/create" && req.method == "POST") {
            auto body = parse_body_or_empty(req);
            std::string meta_pw = body.get("dir_password").as_str(req.header("password"));
            auto gv = vfs_.resolve_user_path(user, body.get("path").as_str());
            if (!gv || !vfs_.can_read_path(user, *gv, meta_pw)) {
                json_err(resp, 403, "Cannot share inaccessible path");
                return;
            }
            auto obj = vfs_.get_obj(&user, *gv, meta_pw, effective_base_url());
            if (!obj.ok) { json_err(resp, 404, obj.error); return; }

            ShareRecord sh;
            sh.share_id = random_base64url(9);
            sh.creator_id = user.id;
            sh.creator_name = user.username;
            sh.path = *gv;
            sh.is_dir = obj.entry.is_dir;
            std::string pw = body.get("password").as_str();
            if (!pw.empty()) sh.password_hash = hash_password_pbkdf2(pw);
            // Clamp expires_hours to [0, 87600] (10 years) to prevent signed integer overflow
            int64_t exp_h = std::clamp<int64_t>(body.get("expires_hours").as_i64(0), 0, 87600);
            sh.expires_at = (exp_h > 0) ? (unix_now() + exp_h * 3600) : 0;
            sh.max_downloads = std::clamp<int64_t>(body.get("max_downloads").as_i64(0), 0, 1000000000LL);

            if (!db_.create_share(sh)) { json_err(resp, 500, "Failed creating share"); return; }
            json_ok(resp, JsonValue::object({{"share_id", sh.share_id}}));
            return;
        }
        if (req.path == "/api/share/list") {
            auto list = db_.list_shares(user.is_admin() ? std::nullopt : std::optional<int64_t>(user.id));
            JsonValue arr = JsonValue::array();
            for (const auto& s : list) {
                arr.push_back(JsonValue::object({
                    {"share_id",      s.share_id},
                    {"path",          s.path},
                    {"is_dir",        s.is_dir},
                    {"creator",       s.creator_name},
                    {"has_password",  s.has_password},
                    {"expires_at",    s.expires_at},
                    {"max_downloads", s.max_downloads},
                    {"downloads",     s.downloads},
                    {"views",         s.views},
                    {"disabled",      s.disabled},
                }));
            }
            json_ok(resp, JsonValue::object({{"content", std::move(arr)}, {"total", static_cast<int64_t>(list.size())}}));
            return;
        }
        if (req.path == "/api/share/delete" && req.method == "POST") {
            auto body = parse_body_or_empty(req);
            std::string sid = body.get("share_id").as_str(req.query_param("id"));
            db_.delete_share(sid, user.is_admin() ? std::nullopt : std::optional<int64_t>(user.id));
            json_ok(resp);
            return;
        }

        if (req.path == "/api/task/undone" || req.path == "/api/task/done") {
            bool done = (req.path == "/api/task/done");
            auto list = tasks_.list_tasks(req.query_param("type"), done);
            JsonValue arr = JsonValue::array();
            for (const auto& t : list) {
                if (!user.is_admin() && t.creator != user.username) continue;
                arr.push_back(JsonValue::object({
                    {"id",       t.id},
                    {"type",     t.type},
                    {"name",     t.name},
                    {"creator",  t.creator},
                    {"state",    t.state},
                    {"status",   t.status_text},
                    {"progress", t.progress},
                    {"error",    t.error_msg},
                }));
            }
            json_ok(resp, std::move(arr));
            return;
        }
        if (req.path == "/api/task/cancel" && req.method == "POST") {
            std::string tid = req.query_param("tid", parse_body_or_empty(req).get("tid").as_str());
            std::string owner = user.is_admin() ? "" : user.username;
            if (!tasks_.cancel_task(tid, owner)) {
                json_err(resp, 404, "Task not found or access denied");
                return;
            }
            json_ok(resp);
            return;
        }
        if (req.path == "/api/task/clear_done" && req.method == "POST") {
            std::string owner = user.is_admin() ? "" : user.username;
            tasks_.clear_done_tasks(req.query_param("type"), owner);
            json_ok(resp);
            return;
        }
    }

    // ========================================================================
    // 9. Admin Control Plane (/api/admin/* — Strictly Admin Identity = 2 Only!)
    // ========================================================================
    if (req.path.starts_with("/api/admin/")) {
        if (!user.is_admin()) {
            json_err(resp, 403, "Administrator privileges required");
            return;
        }

        if (req.path == "/api/admin/driver/list") {
            JsonValue arr = JsonValue::array();
            for (const auto& d : get_registered_drivers()) {
                JsonValue fields = JsonValue::array();
                for (const auto& f : d.fields) {
                    fields.push_back(JsonValue::object({
                        {"name",        f.name},
                        {"label",       f.label},
                        {"type",        f.type},
                        {"default_val", f.default_val},
                        {"options",     f.options},
                        {"required",    f.required},
                        {"help_text",   f.help_text},
                    }));
                }
                arr.push_back(JsonValue::object({
                    {"name",         d.name},
                    {"display_name", d.display_name},
                    {"local_sort",   d.local_sort},
                    {"no_cache",     d.no_cache},
                    {"no_upload",    d.no_upload},
                    {"fields",       std::move(fields)},
                }));
            }
            json_ok(resp, std::move(arr));
            return;
        }
        if (req.path == "/api/admin/driver/names") {
            JsonValue arr = JsonValue::array();
            for (const auto& d : get_registered_drivers()) arr.push_back(d.name);
            json_ok(resp, std::move(arr));
            return;
        }

        if (req.path == "/api/admin/system/uptime" && req.method == "GET") {
            const auto origin = req.query_param("origin");
            if (!valid_monitor_origin(origin)) { json_err(resp, 400, "Invalid monitor origin"); return; }
            OutboundRequest outgoing;
            outgoing.url = origin + "/api/status";
            outgoing.connect_timeout_ms = 3000;
            outgoing.timeout_ms = 8000;
            std::string body;
            outgoing.on_data = [&](const char* data, size_t len) {
                if (body.size() + len > 4096) return false;
                body.append(data, len); return true;
            };
            auto result = HttpClient::instance().perform(outgoing);
            auto status = JsonValue::parse(body);
            if (!result.ok || result.status_code != 200 || !status || !status->is_object()) {
                json_err(resp, 502, "Monitor unavailable"); return;
            }
            auto state = status->get("state").as_str();
            const auto samples = status->get("samples").as_i64(-1);
            const auto successful = status->get("successful").as_i64(-1);
            const auto& checked = status->get("checkedAt");
            if ((state != "up" && state != "down" && state != "stale" && state != "unknown") ||
                !status->get("samples").is_int() || !status->get("successful").is_int() ||
                samples < 0 || samples > 288 || successful < 0 || successful > samples ||
                !(checked.is_null() || (checked.is_int() && checked.as_i64() >= 0))) {
                json_err(resp, 502, "Invalid monitor response"); return;
            }
            resp.headers["Cache-Control"] = "no-store";
            json_ok(resp, JsonValue::object({{"state", state}, {"samples", samples},
                {"successful", successful}, {"checkedAt", checked}}));
            return;
        }

        if (req.path == "/api/admin/system/stats") {
            auto pt = read_proc_telemetry();
            HttpServer::Stats hs = server_ ? server_->stats() : HttpServer::Stats{};
            int64_t started = hs.started_at_unix > 0 ? hs.started_at_unix : boot_unix_;
            int64_t uptime = std::max<int64_t>(0, unix_now() - started);
            auto undone = tasks_.list_tasks("", false);
            auto done   = tasks_.list_tasks("", true);
            json_ok(resp, JsonValue::object({
                {"app_name",           kAppName},
                {"author",             kAuthor},
                {"author_url",         kAuthorUrl},
                {"version",            kVersion},
                {"engine",             kEngine},
                {"uptime_sec",         uptime},
                {"rss_bytes",          pt.rss_bytes},
                {"peak_rss_bytes",     pt.peak_rss_bytes},
                {"threads",            pt.threads},
                {"cpu_cores",          static_cast<int64_t>(std::max(1u, std::thread::hardware_concurrency()))},
                {"active_connections", hs.active_connections},
                {"total_requests",     static_cast<int64_t>(hs.total_requests)},
                {"bytes_sent",         static_cast<int64_t>(hs.bytes_sent)},
                {"bytes_received",     static_cast<int64_t>(hs.bytes_received)},
                {"vfs_cache_hits",     static_cast<int64_t>(vfs_.cache_hits())},
                {"vfs_cache_misses",   static_cast<int64_t>(vfs_.cache_misses())},
                {"vfs_cache_entries",  static_cast<int64_t>(vfs_.cache_size())},
                {"mounted_storages",   static_cast<int64_t>(vfs_.mounted_count())},
                {"db_size_bytes",      db_.db_file_size_bytes()},
                {"fts_indexed_files",  db_.count_search_index()},
                {"tasks_undone",       static_cast<int64_t>(undone.size())},
                {"tasks_done",         static_cast<int64_t>(done.size())},
            }));
            return;
        }

        // Storages CRUD
        if (req.path == "/api/admin/storage/get") {
            int64_t id = parse_i64(req.query_param("id")).value_or(0);
            auto s = db_.get_storage(id);
            if (!s) { json_err(resp, 404, "Storage not found"); return; }
            json_ok(resp, JsonValue::object({
                {"id",               s->id},
                {"mount_path",       s->mount_path},
                {"order",            s->order_idx},
                {"driver",           s->driver},
                {"cache_expiration", s->cache_expiration},
                {"status",           s->status},
                {"addition",         mask_sensitive_json(s->addition)},
                {"remark",           s->remark},
                {"disabled",         s->disabled},
                {"enable_sign",      s->enable_sign},
                {"order_by",         s->order_by},
                {"order_direction",  s->order_direction},
                {"web_proxy",        s->web_proxy},
                {"webdav_policy",    s->webdav_policy},
            }));
            return;
        }
        if (req.path == "/api/admin/storage/list") {
            auto list = db_.list_storages(false);
            JsonValue arr = JsonValue::array();
            for (const auto& s : list) {
                arr.push_back(JsonValue::object({
                    {"id",               s.id},
                    {"mount_path",       s.mount_path},
                    {"order",            s.order_idx},
                    {"driver",           s.driver},
                    {"cache_expiration", s.cache_expiration},
                    {"status",           s.status},
                    {"addition",         mask_sensitive_json(s.addition)}, // Mask secrets in list response!
                    {"remark",           s.remark},
                    {"disabled",         s.disabled},
                    {"enable_sign",      s.enable_sign},
                    {"order_by",         s.order_by},
                    {"order_direction",  s.order_direction},
                    {"web_proxy",        s.web_proxy},
                    {"webdav_policy",    s.webdav_policy},
                }));
            }
            json_ok(resp, JsonValue::object({{"content", std::move(arr)}, {"total", static_cast<int64_t>(list.size())}}));
            return;
        }
        if (req.path == "/api/admin/storage/create" && req.method == "POST") {
            auto b = parse_body_or_empty(req);
            StorageRecord s;
            s.mount_path       = b.get("mount_path").as_str();
            s.order_idx        = bounded_json_int(b.get("order"), 0, -100000, 100000);
            s.driver           = b.get("driver").as_str();
            s.cache_expiration = bounded_json_int(b.get("cache_expiration"), 30, 0, 525600);
            s.addition         = b.get("addition").is_object() ? b.get("addition").dump() : b.get("addition").as_str("{}");
            s.remark           = b.get("remark").as_str();
            s.enable_sign      = b.get("enable_sign").as_bool(false);
            s.web_proxy        = b.get("web_proxy").as_bool(false);
            s.order_by         = b.get("order_by").as_str("name");
            s.order_direction  = b.get("order_direction").as_str("ASC");

            int64_t id = db_.create_storage(s);
            if (id <= 0) { json_err(resp, 400, "Failed to create storage (invalid or duplicate mount_path)"); return; }
            std::string err;
            vfs_.reload_storage(id, &err);
            json_ok(resp, JsonValue::object({{"id", id}, {"warning", err}}));
            return;
        }
        if (req.path == "/api/admin/storage/update" && req.method == "POST") {
            auto b = parse_body_or_empty(req);
            int64_t id = b.get("id").as_i64(0);
            auto existing = db_.get_storage(id);
            if (!existing) { json_err(resp, 404, "Storage not found"); return; }
            std::string old_mp = existing->mount_path;
            existing->mount_path       = b.get("mount_path").as_str(existing->mount_path);
            existing->order_idx        = bounded_json_int(b.get("order"), existing->order_idx, -100000, 100000);
            existing->driver           = b.get("driver").as_str(existing->driver);
            existing->cache_expiration = bounded_json_int(b.get("cache_expiration"), existing->cache_expiration, 0, 525600);
            existing->remark           = b.get("remark").as_str(existing->remark);
            existing->order_by         = b.get("order_by").as_str(existing->order_by);
            existing->order_direction  = b.get("order_direction").as_str(existing->order_direction);
            if (b.contains("addition")) {
                existing->addition = merge_masked_addition(existing->addition, b.get("addition"));
            }
            existing->disabled    = b.get("disabled").as_bool(existing->disabled);
            existing->enable_sign = b.get("enable_sign").as_bool(existing->enable_sign);
            existing->web_proxy   = b.get("web_proxy").as_bool(existing->web_proxy);
            if (!db_.update_storage(*existing)) { json_err(resp, 400, "Update failed"); return; }
            if (old_mp != existing->mount_path || existing->disabled) vfs_.unload_storage_path(old_mp);
            std::string err;
            if (!existing->disabled) vfs_.reload_storage(id, &err);
            json_ok(resp, JsonValue::object({{"warning", err}}));
            return;
        }
        if (req.path == "/api/admin/storage/delete" && req.method == "POST") {
            int64_t id = parse_i64(req.query_param("id")).value_or(parse_body_or_empty(req).get("id").as_i64(0));
            if (auto s = db_.get_storage(id)) {
                vfs_.unload_storage_path(s->mount_path);
                db_.delete_storage(id);
            }
            json_ok(resp);
            return;
        }
        if (req.path == "/api/admin/storage/load_all" && req.method == "POST") {
            vfs_.reload_all_storages();
            json_ok(resp);
            return;
        }

        // Users CRUD
        if (req.path == "/api/admin/user/list") {
            auto list = db_.list_users();
            JsonValue arr = JsonValue::array();
            for (const auto& u : list) {
                JsonValue rids = JsonValue::array();
                for (int64_t r : u.role_ids) rids.push_back(r);
                arr.push_back(JsonValue::object({
                    {"id",        u.id},
                    {"username",  u.username},
                    {"base_path", u.base_path},
                    {"role",      u.identity},
                    {"role_ids",  std::move(rids)},
                    {"disabled",  u.disabled},
                    {"otp",       !u.otp_secret.empty()},
                }));
            }
            json_ok(resp, JsonValue::object({{"content", std::move(arr)}, {"total", static_cast<int64_t>(list.size())}}));
            return;
        }
        if (req.path == "/api/admin/user/create" && req.method == "POST") {
            auto b = parse_body_or_empty(req);
            std::string uname = b.get("username").as_str();
            std::string pwd   = b.get("password").as_str();
            if (!is_valid_filename(uname) || pwd.size() < 6) {
                json_err(resp, 400, "Invalid username or password too short (min 6 chars)");
                return;
            }
            UserRecord nu;
            nu.username = uname;
            nu.password_hash = hash_password_pbkdf2(pwd);
            nu.pwd_ts = unix_now();
            nu.base_path = clean_vpath(b.get("base_path").as_sv("/")).value_or("/");
            nu.identity = kUserGeneral;
            for (const auto& rv : b.get("role_ids").as_array()) nu.role_ids.push_back(rv.as_i64(0));
            if (nu.role_ids.empty()) nu.role_ids.push_back(2);
            nu.disabled = b.get("disabled").as_bool(false);
            int64_t id = db_.create_user(nu);
            if (id <= 0) { json_err(resp, 400, "Failed creating user"); return; }
            json_ok(resp, JsonValue::object({{"id", id}}));
            return;
        }
        if (req.path == "/api/admin/user/update" && req.method == "POST") {
            auto b = parse_body_or_empty(req);
            int64_t id = b.get("id").as_i64(0);
            auto eu = db_.get_user_by_id(id);
            if (!eu) { json_err(resp, 404, "User not found"); return; }
            bool next_disabled = b.get("disabled").as_bool(eu->disabled);
            if (eu->is_admin() && next_disabled) {
                json_err(resp, 400, "Cannot disable the administrator account");
                return;
            }
            if (b.contains("password") && !b.get("password").as_sv().empty() && b.get("password").as_sv().size() < 6) {
                json_err(resp, 400, "Password must be at least 6 characters");
                return;
            }
            eu->base_path = clean_vpath(b.get("base_path").as_sv(eu->base_path)).value_or("/");
            eu->disabled  = next_disabled;
            if (b.contains("role_ids") && b.get("role_ids").is_array()) {
                eu->role_ids.clear();
                for (const auto& rv : b.get("role_ids").as_array()) eu->role_ids.push_back(rv.as_i64(0));
            }
            db_.update_user(*eu);
            if (b.contains("password") && !b.get("password").as_sv().empty()) {
                db_.update_user_password(id, b.get("password").as_sv());
            }
            json_ok(resp);
            return;
        }
        if (req.path == "/api/admin/user/cancel_2fa" && req.method == "POST") {
            int64_t id = parse_i64(req.query_param("id")).value_or(parse_body_or_empty(req).get("id").as_i64(0));
            db_.update_user_otp(id, "", -1);
            json_ok(resp);
            return;
        }
        if (req.path == "/api/admin/user/delete" && req.method == "POST") {
            int64_t id = parse_i64(req.query_param("id")).value_or(parse_body_or_empty(req).get("id").as_i64(0));
            if (!db_.delete_user(id)) { json_err(resp, 400, "Cannot delete built-in admin/guest or missing user"); return; }
            json_ok(resp);
            return;
        }

        // Roles CRUD
        if (req.path == "/api/admin/role/list") {
            auto list = db_.list_roles();
            JsonValue arr = JsonValue::array();
            for (const auto& r : list) {
                JsonValue sc = JsonValue::array();
                for (const auto& s : r.scopes) {
                    sc.push_back(JsonValue::object({{"path", s.path}, {"permission", s.permission}}));
                }
                arr.push_back(JsonValue::object({
                    {"id", r.id}, {"name", r.name}, {"description", r.description}, {"permission_scopes", std::move(sc)}
                }));
            }
            json_ok(resp, JsonValue::object({{"content", std::move(arr)}, {"total", static_cast<int64_t>(list.size())}}));
            return;
        }
        if (req.path == "/api/admin/role/create" && req.method == "POST") {
            auto b = parse_body_or_empty(req);
            RoleRecord r;
            r.name = b.get("name").as_str();
            r.description = b.get("description").as_str();
            for (const auto& sv : b.get("permission_scopes").as_array()) {
                r.scopes.push_back(RoleScope{
                    clean_vpath(sv.get("path").as_sv("/")).value_or("/"),
                    static_cast<int32_t>(sv.get("permission").as_i64(0))
                });
            }
            int64_t id = db_.create_role(r);
            if (id <= 0) { json_err(resp, 400, "Failed creating role"); return; }
            json_ok(resp, JsonValue::object({{"id", id}}));
            return;
        }
        if (req.path == "/api/admin/role/update" && req.method == "POST") {
            auto b = parse_body_or_empty(req);
            RoleRecord r;
            r.id = b.get("id").as_i64(0);
            r.name = b.get("name").as_str();
            r.description = b.get("description").as_str();
            for (const auto& sv : b.get("permission_scopes").as_array()) {
                r.scopes.push_back(RoleScope{
                    clean_vpath(sv.get("path").as_sv("/")).value_or("/"),
                    static_cast<int32_t>(sv.get("permission").as_i64(0))
                });
            }
            db_.update_role(r);
            json_ok(resp);
            return;
        }
        if (req.path == "/api/admin/role/delete" && req.method == "POST") {
            int64_t id = parse_i64(req.query_param("id")).value_or(parse_body_or_empty(req).get("id").as_i64(0));
            if (!db_.delete_role(id)) { json_err(resp, 400, "Cannot delete built-in role"); return; }
            json_ok(resp);
            return;
        }

        // Metas CRUD
        if (req.path == "/api/admin/meta/list") {
            auto list = db_.list_metas();
            JsonValue arr = JsonValue::array();
            for (const auto& m : list) {
                arr.push_back(JsonValue::object({
                    {"id", m.id}, {"path", m.path}, {"password", m.password.empty() ? "" : "********"},
                    {"p_sub", m.p_sub}, {"write", m.write}, {"w_sub", m.w_sub},
                    {"hide", m.hide}, {"h_sub", m.h_sub}, {"readme", m.readme}, {"r_sub", m.r_sub}
                }));
            }
            json_ok(resp, JsonValue::object({{"content", std::move(arr)}, {"total", static_cast<int64_t>(list.size())}}));
            return;
        }
        if (req.path == "/api/admin/meta/create" && req.method == "POST") {
            auto b = parse_body_or_empty(req);
            MetaRecord m;
            m.path     = b.get("path").as_str("/");
            m.password = b.get("password").as_str();
            m.p_sub    = b.get("p_sub").as_bool(false);
            m.write    = b.get("write").as_bool(false);
            m.w_sub    = b.get("w_sub").as_bool(false);
            m.hide     = b.get("hide").as_str();
            m.h_sub    = b.get("h_sub").as_bool(false);
            m.readme   = b.get("readme").as_str();
            m.r_sub    = b.get("r_sub").as_bool(false);
            int64_t id = db_.create_meta(m);
            if (id <= 0) { json_err(resp, 400, "Failed creating meta (invalid or duplicate path)"); return; }
            vfs_.invalidate_cache(m.path);
            json_ok(resp, JsonValue::object({{"id", id}}));
            return;
        }
        if (req.path == "/api/admin/meta/update" && req.method == "POST") {
            auto b = parse_body_or_empty(req);
            int64_t id = b.get("id").as_i64(0);
            auto ex = db_.get_meta(id);
            if (!ex) { json_err(resp, 404, "Meta rule not found"); return; }
            std::string old_path = ex->path;
            ex->path = b.get("path").as_str(ex->path);
            if (b.contains("password")) {
                std::string np = b.get("password").as_str();
                if (np != "********") ex->password = np;
            }
            ex->p_sub  = b.get("p_sub").as_bool(ex->p_sub);
            ex->write  = b.get("write").as_bool(ex->write);
            ex->w_sub  = b.get("w_sub").as_bool(ex->w_sub);
            ex->hide   = b.get("hide").as_str(ex->hide);
            ex->h_sub  = b.get("h_sub").as_bool(ex->h_sub);
            ex->readme = b.get("readme").as_str(ex->readme);
            ex->r_sub  = b.get("r_sub").as_bool(ex->r_sub);
            if (!db_.update_meta(*ex)) { json_err(resp, 400, "Failed updating meta"); return; }
            vfs_.invalidate_cache(old_path);
            vfs_.invalidate_cache(ex->path);
            json_ok(resp);
            return;
        }
        if (req.path == "/api/admin/meta/delete" && req.method == "POST") {
            int64_t id = parse_i64(req.query_param("id")).value_or(parse_body_or_empty(req).get("id").as_i64(0));
            auto ex = db_.get_meta(id);
            db_.delete_meta(id);
            if (ex) vfs_.invalidate_cache(ex->path);
            json_ok(resp);
            return;
        }

        // Settings & Search Index
        if (req.path == "/api/admin/setting/list") {
            auto m = db_.list_settings(false);
            JsonValue obj = JsonValue::object();
            for (const auto& [k, v] : m) {
                obj[k] = (k == "token") ? "********" : v;
            }
            json_ok(resp, std::move(obj));
            return;
        }
        if (req.path == "/api/admin/setting/save" && req.method == "POST") {
            auto b = parse_body_or_empty(req);
            if (b.is_array()) {
                for (const auto& item : b.as_array()) {
                    std::string k = std::string(trim_ascii(item.get("key").as_sv()));
                    std::string v = item.get("value").as_str();
                    if (k.empty() || k.size() > 64 || v.size() > 16384 || k == "token" || k == "version") continue;
                    // Clamp numeric settings to safe operational ranges
                    if (k == "default_page_size") {
                        v = std::to_string(std::clamp<int64_t>(parse_i64(v).value_or(50), 10, 500));
                    } else if (k == "link_expiration") {
                        v = std::to_string(std::clamp<int64_t>(parse_i64(v).value_or(0), 0, 87600));
                    } else if (k == "max_devices") {
                        v = std::to_string(std::clamp<int64_t>(parse_i64(v).value_or(0), 0, 1000));
                    } else if (k == "device_session_ttl") {
                        v = std::to_string(std::clamp<int64_t>(parse_i64(v).value_or(86400), 60, 31536000));
                    }
                    bool is_pub = (k == "site_title" || k == "announcement" || k == "pagination_type" ||
                                   k == "default_page_size" || k == "allow_indexed" || k == "allow_mounted" ||
                                   k == "robots_txt" || k == "search_index" || k == "enable_search");
                    db_.set_setting(k, v, is_pub ? "site" : "global", is_pub);
                }
            }
            json_ok(resp);
            return;
        }
        if (req.path == "/api/admin/setting/reset_token" && req.method == "POST") {
            std::string nt = "kova-" + random_hex(24);
            db_.set_setting("token", nt, "global", false);
            json_ok(resp, JsonValue::object({{"token", nt}}));
            return;
        }
        if (req.path == "/api/admin/index/build" && req.method == "POST") {
            int64_t cnt = vfs_.rebuild_search_index("/", 6);
            json_ok(resp, JsonValue::object({{"indexed", cnt}}));
            return;
        }
        if (req.path == "/api/admin/index/clear" && req.method == "POST") {
            db_.clear_search_index();
            json_ok(resp);
            return;
        }
        if (req.path == "/api/admin/index/progress") {
            json_ok(resp, JsonValue::object({
                {"obj_count", db_.count_search_index()},
                {"is_done",   true},
            }));
            return;
        }
        if (req.path == "/api/admin/session/list") {
            auto list = db_.list_sessions();
            JsonValue arr = JsonValue::array();
            for (const auto& s : list) {
                arr.push_back(JsonValue::object({
                    {"user_id", s.user_id}, {"device_key", s.device_key}, {"user_agent", s.user_agent},
                    {"ip", s.ip}, {"last_active", s.last_active}, {"status", s.status}
                }));
            }
            json_ok(resp, std::move(arr));
            return;
        }
    }

    json_err(resp, 404, "API endpoint not found: " + req.path, 404);
}

} // namespace alist
