#include "alist/webdav_server.hpp"
#include "alist/common.hpp"
#include "alist/crypto.hpp"
#include "alist/http_client.hpp"
#include "alist/xml.hpp"

namespace alist {

void WebDavServer::serve_resolution(const HttpRequest& req, HttpResponse& resp,
                                    const DownloadResolution& dl, bool force_proxy,
                                    bool force_attachment) {
    // Hardened anti-XSS headers on ALL user file downloads/previews!
    resp.headers["Referrer-Policy"] = "no-referrer";
    resp.headers["X-Content-Type-Options"] = "nosniff";
    resp.headers["Content-Security-Policy"] =
        "default-src 'none'; img-src 'self' data:; media-src 'self'; style-src 'unsafe-inline'; sandbox";

    bool active_content = is_potentially_active_web_content(dl.file_meta.name);
    std::string disp_type = (force_attachment || active_content) ? "attachment" : "inline";
    if (!dl.file_meta.name.empty() && dl.file_meta.name != "/") {
        resp.headers["Content-Disposition"] =
            disp_type + "; filename*=UTF-8''" + url_encode(dl.file_meta.name);
    }

    if (dl.mode == DownloadResolution::Mode::LocalFile) {
        resp.headers["Content-Type"] = active_content ? "application/octet-stream" : dl.mime_type;
        resp.sendfile_path  = dl.local_path;
        resp.sendfile_size  = dl.file_meta.size;
        resp.sendfile_mtime = dl.file_meta.modified;
        resp.sendfile_etag  = dl.file_meta.etag;
        return;
    }

    if (dl.mode == DownloadResolution::Mode::RedirectUrl && !force_proxy) {
        resp.headers["Cache-Control"] = "no-store";
        resp.redirect(302, dl.url);
        return;
    }

    if (dl.mode == DownloadResolution::Mode::CustomStream && dl.stream_fn) {
        resp.headers["Content-Type"] = active_content ? "application/octet-stream" : dl.mime_type;
        resp.headers["Accept-Ranges"] = "bytes";
        int64_t total = dl.file_meta.size;
        bool unsat = false;
        auto rng = parse_http_range(req.header("range"), total, &unsat);
        if (unsat) {
            resp.status = 416;
            resp.headers["Content-Range"] = "bytes */" + std::to_string(total);
            return;
        }
        int64_t off = rng ? rng->start : 0;
        int64_t len = rng ? (rng->end - rng->start + 1) : total;
        if (rng) {
            resp.status = 206;
            resp.headers["Content-Range"] = "bytes " + std::to_string(rng->start) + "-" +
                                            std::to_string(rng->end) + "/" + std::to_string(total);
        } else {
            resp.status = 200;
        }
        resp.stream_content_length = len;
        auto fn = dl.stream_fn;
        resp.stream_writer = [fn, off, len](const std::function<bool(const char*, size_t)>& sink) {
            return fn(off, len, sink);
        };
        return;
    }

    // UpstreamHttp or forced-proxy RedirectUrl
    resp.headers["Content-Type"] = active_content ? "application/octet-stream" : dl.mime_type;
    std::string range_hdr = req.header("range");
    if (!range_hdr.empty()) {
        resp.status = 206;
    } else {
        resp.status = 200;
    }
    resp.stream_content_length = -1; // Streamed via HTTP/1.1 chunked encoding
    DownloadResolution copy_dl = dl;
    resp.stream_writer = [copy_dl, range_hdr](const std::function<bool(const char*, size_t)>& sink) -> bool {
        OutboundRequest ureq;
        ureq.url = copy_dl.url;
        ureq.headers = copy_dl.headers;
        if (!range_hdr.empty()) ureq.headers["Range"] = range_hdr;
        ureq.username = copy_dl.username;
        ureq.password = copy_dl.password;
        ureq.ssh_private_key_file = copy_dl.ssh_key_file;
        ureq.allow_private_net = copy_dl.allow_private_net;
        ureq.on_data = [&](const char* d, size_t n) -> bool {
            return sink(d, n);
        };
        auto uresp = HttpClient::instance().perform(ureq);
        return uresp.ok && uresp.status_code >= 200 && uresp.status_code < 400;
    };
}

WebDavServer::WebDavServer(VfsEngine& vfs, BruteForceProtector& bf, TokenBucket& up_limiter)
    : vfs_(vfs), bf_(bf), up_limiter_(up_limiter) {}

std::optional<UserRecord> WebDavServer::authenticate_basic_or_bearer(const HttpRequest& req, HttpResponse& resp) {
    auto challenge = [&]() -> std::optional<UserRecord> {
        resp.status = 401;
        resp.headers["WWW-Authenticate"] = R"(Basic realm="Kova WebDAV")";
        return std::nullopt;
    };

    if (bf_.is_blocked("dav:" + req.remote_ip)) {
        resp.status = 429;
        resp.body = "Too many failed authentication attempts";
        return std::nullopt;
    }

    std::string auth = req.header("authorization");
    if (auth.empty()) {
        auto guest = vfs_.db().get_guest_user();
        if (guest && !guest->disabled) return guest;
        return challenge();
    }

    if (auth.starts_with("Basic ") || auth.starts_with("basic ")) {
        auto dec = base64_decode(trim_ascii(std::string_view(auth).substr(6)));
        if (!dec) {
            bf_.record_failure("dav:" + req.remote_ip);
            return challenge();
        }
        auto colon = dec->find(':');
        if (colon == std::string::npos) {
            bf_.record_failure("dav:" + req.remote_ip);
            return challenge();
        }
        std::string uname = dec->substr(0, colon);
        std::string pwd   = dec->substr(colon + 1);
        auto u = vfs_.db().get_user_by_name(uname);
        if (!u || u->is_guest() || u->disabled || !verify_password_pbkdf2(pwd, u->password_hash)) {
            bf_.record_failure("dav:" + req.remote_ip);
            return challenge();
        }
        // Security: Never allow WebDAV Basic Auth to bypass 2FA when TOTP is enabled on the account!
        if (!u->otp_secret.empty()) {
            std::string otp = req.header("x-otp-code");
            int64_t step = -1;
            if (otp.empty() || !totp_verify(u->otp_secret, otp, unix_now(), &step) ||
                !vfs_.db().verify_and_advance_otp_step(u->id, step)) {
                bf_.record_failure("dav:" + req.remote_ip);
                resp.headers["X-2FA-Required"] = "true";
                return challenge();
            }
        }
        bf_.record_success("dav:" + req.remote_ip);
        return u;
    }

    // Strip optional RFC 6750 "Bearer " prefix
    std::string_view tok = trim_ascii(auth);
    if (tok.size() > 7 && (tok.starts_with("Bearer ") || tok.starts_with("bearer "))) {
        tok = trim_ascii(tok.substr(7));
    }

    // JWT or Admin static token
    std::string sys_tok = vfs_.db().get_setting("token", "");
    if (!sys_tok.empty() && constant_time_equals(tok, sys_tok)) {
        return vfs_.db().get_admin_user();
    }
    auto claims = jwt_decode_hs256(tok, vfs_.config().jwt_secret);
    if (!claims || vfs_.db().is_token_jti_revoked(claims->jti)) {
        bf_.record_failure("dav:" + req.remote_ip);
        return challenge();
    }
    auto u = vfs_.db().get_user_by_name(claims->username);
    if (!u || u->disabled || u->pwd_ts != claims->pwd_ts) return challenge();
    return u;
}

void WebDavServer::handle(HttpRequest& req, HttpResponse& resp) {
    if (req.method == "OPTIONS") {
        resp.status = 200;
        resp.headers["DAV"] = "1, 2";
        resp.headers["MS-Author-Via"] = "DAV";
        resp.headers["Allow"] = "OPTIONS, GET, HEAD, POST, PUT, DELETE, PROPFIND, PROPPATCH, MKCOL, COPY, MOVE, LOCK, UNLOCK";
        return;
    }

    auto user_opt = authenticate_basic_or_bearer(req, resp);
    if (!user_opt) return;
    const UserRecord& user = *user_opt;

    // Strip "/dav" prefix
    std::string_view rel = req.path;
    if (rel.starts_with("/dav")) rel.remove_prefix(4);
    if (rel.empty()) rel = "/";

    auto global_vpath_opt = vfs_.resolve_user_path(user, rel);
    if (!global_vpath_opt) {
        resp.status = 403;
        return;
    }
    const std::string& gvpath = *global_vpath_opt;

    int32_t perms = vfs_.effective_permissions(user, gvpath);
    if (!user.is_admin() && !has_perm(perms, PermWebdavRead)) {
        resp.status = 403;
        return;
    }

    auto build_prop_response = [](std::string_view href_path, const FileObj& f) -> std::string {
        std::string enc_href = url_encode(href_path, true);
        if (f.is_dir && !enc_href.ends_with('/')) enc_href.push_back('/');
        std::string out = "<D:response><D:href>" + xml_escape(enc_href) + "</D:href><D:propstat><D:prop>";
        out += "<D:displayname>" + xml_escape(f.name == "/" ? "root" : f.name) + "</D:displayname>";
        out += "<D:getlastmodified>" + format_http_date(f.modified > 0 ? f.modified : unix_now()) + "</D:getlastmodified>";
        if (f.is_dir) {
            out += "<D:resourcetype><D:collection/></D:resourcetype>";
        } else {
            out += "<D:resourcetype/>";
            out += "<D:getcontentlength>" + std::to_string(f.size) + "</D:getcontentlength>";
            out += "<D:getcontenttype>" + xml_escape(guess_mime_type(f.name)) + "</D:getcontenttype>";
            if (!f.etag.empty()) out += "<D:getetag>" + xml_escape(f.etag) + "</D:getetag>";
        }
        out += "</D:prop><D:status>HTTP/1.1 200 OK</D:status></D:propstat></D:response>";
        return out;
    };

    std::string meta_pw = req.header("password");

    if (req.method == "PROPFIND") {
        std::string depth = req.header("depth", "1");
        auto self_res = vfs_.get_obj(&user, gvpath, meta_pw, req.base_url());
        if (!self_res.ok) {
            resp.status = self_res.http_status;
            return;
        }
        std::string user_href = clean_vpath("/dav/" + std::string(rel)).value_or("/dav");
        std::string xml = R"(<?xml version="1.0" encoding="utf-8"?><D:multistatus xmlns:D="DAV:">)";
        xml += build_prop_response(user_href, self_res.entry);

        if (self_res.entry.is_dir && depth != "0") {
            auto ls = vfs_.list_dir(&user, gvpath, meta_pw, false);
            if (ls.ok) {
                for (const auto& child : ls.entries) {
                    std::string ch_href = join_vpath(user_href, child.name).value_or(user_href + "/" + child.name);
                    xml += build_prop_response(ch_href, child);
                }
            }
        }
        xml += "</D:multistatus>";
        resp.status = 207;
        resp.headers["Content-Type"] = "application/xml; charset=utf-8";
        resp.body = std::move(xml);
        return;
    }

    if (req.method == "GET" || req.method == "HEAD") {
        int st_code = 200;
        std::string rerr;
        if (!vfs_.can_read_path(user, gvpath, meta_pw, &st_code, &rerr)) {
            resp.status = st_code;
            return;
        }
        std::string derr;
        auto dl = vfs_.resolve_download(gvpath, req.headers, derr);
        if (!dl) {
            resp.status = 404;
            return;
        }
        serve_resolution(req, resp, *dl, false, false);
        return;
    }

    // Mutating WebDAV methods require PermWebdavManage!
    if (!user.is_admin() && !has_perm(perms, PermWebdavManage)) {
        resp.status = 403;
        return;
    }

    if (req.method == "PUT") {
        auto [_, fname] = split_vpath(gvpath);
        UploadStream up;
        up.filename = fname;
        up.size = req.is_chunked ? -1 : req.content_length;
        up.mime_type = req.header("content-type", std::string(guess_mime_type(fname)));
        int64_t total_read = 0;
        int64_t max_up = vfs_.config().max_upload_bytes;
        up.read_fn = [&](char* buf, size_t max_len) -> ssize_t {
            if (!req.stream_body) return 0;
            ssize_t n = req.stream_body(buf, max_len);
            if (n > 0) {
                total_read += n;
                if (total_read > max_up) return -1;
                up_limiter_.consume(static_cast<size_t>(n));
            }
            return n;
        };
        std::string err;
        if (!vfs_.put_stream(gvpath, up, err)) {
            resp.status = 409;
            resp.body = err;
            return;
        }
        resp.status = 201;
        return;
    }

    if (req.method == "MKCOL") {
        std::string err;
        resp.status = vfs_.mkdir(gvpath, err) ? 201 : 405;
        return;
    }

    if (req.method == "DELETE") {
        std::string err;
        resp.status = vfs_.remove(gvpath, err) ? 204 : 404;
        return;
    }

    if (req.method == "MOVE" || req.method == "COPY") {
        std::string dest = req.header("destination");
        if (dest.empty()) { resp.status = 400; return; }
        // Extract path from full Destination URL if needed
        if (auto p = dest.find("://"); p != std::string::npos) {
            auto sl = dest.find('/', p + 3);
            dest = (sl == std::string::npos) ? "/" : dest.substr(sl);
        }
        auto dec_dest = url_decode(dest);
        if (!dec_dest || !dec_dest->starts_with("/dav")) { resp.status = 400; return; }
        std::string_view drel = std::string_view(*dec_dest).substr(4);
        if (drel.empty()) drel = "/";
        auto dst_global = vfs_.resolve_user_path(user, drel);
        if (!dst_global || !vfs_.is_path_in_role_scope(user, *dst_global)) {
            resp.status = 403;
            return;
        }
        int32_t dst_perms = vfs_.effective_permissions(user, *dst_global);
        if (!user.is_admin() && !has_perm(dst_perms, PermWebdavManage)) {
            resp.status = 403;
            return;
        }
        auto [src_par, _] = split_vpath(gvpath);
        auto [dst_par, dst_name] = split_vpath(*dst_global);
        std::string err;
        bool ok = false;
        if (req.method == "MOVE") {
            if (src_par == dst_par) {
                ok = vfs_.rename_obj(gvpath, dst_name, err);
            } else {
                ok = vfs_.move_obj(gvpath, dst_par, err);
                if (ok) {
                    auto [__, src_name] = split_vpath(gvpath);
                    if (src_name != dst_name) {
                        if (auto moved_p = join_vpath(dst_par, src_name)) {
                            ok = vfs_.rename_obj(*moved_p, dst_name, err);
                        }
                    }
                }
            }
        } else {
            ok = vfs_.copy_obj_sync(gvpath, dst_par, err);
        }
        resp.status = ok ? 201 : 500;
        return;
    }

    if (req.method == "LOCK") {
        std::string tok = "opaquelocktoken:" + random_hex(16);
        {
            std::lock_guard<std::mutex> lk(locks_mu_);
            if (locks_.size() > 4096) locks_.clear();
            locks_[gvpath] = DavLock{tok, user.username, unix_now() + 3600};
        }
        resp.status = 200;
        resp.headers["Lock-Token"] = "<" + tok + ">";
        resp.headers["Content-Type"] = "application/xml; charset=utf-8";
        resp.body = R"(<?xml version="1.0" encoding="utf-8"?><D:prop xmlns:D="DAV:"><D:lockdiscovery><D:activelock>)"
                    R"(<D:locktype><D:write/></D:locktype><D:lockscope><D:exclusive/></D:lockscope><D:depth>infinity</D:depth>)"
                    "<D:timeout>Second-3600</D:timeout><D:locktoken><D:href>" + tok +
                    "</D:href></D:locktoken></D:activelock></D:lockdiscovery></D:prop>";
        return;
    }

    if (req.method == "UNLOCK") {
        std::lock_guard<std::mutex> lk(locks_mu_);
        locks_.erase(gvpath);
        resp.status = 204;
        return;
    }

    if (req.method == "PROPPATCH") {
        resp.status = 207;
        resp.headers["Content-Type"] = "application/xml; charset=utf-8";
        resp.body = R"(<?xml version="1.0" encoding="utf-8"?><D:multistatus xmlns:D="DAV:"></D:multistatus>)";
        return;
    }

    resp.status = 405;
}

} // namespace alist
