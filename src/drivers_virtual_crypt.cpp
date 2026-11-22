#include "alist/common.hpp"
#include "alist/crypto.hpp"
#include "alist/driver.hpp"
#include "alist/http_client.hpp"
#include "alist/json.hpp"
#include "alist/security.hpp"
#include "alist/vfs.hpp"
#include "c_libs.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace alist {

// Forward declarations from other driver translation units
std::unique_ptr<StorageDriver> make_local_driver();
DriverConfigInfo local_driver_info();
std::unique_ptr<StorageDriver> make_s3_driver();
DriverConfigInfo s3_driver_info();
std::unique_ptr<StorageDriver> make_webdav_driver();
DriverConfigInfo webdav_driver_info();
std::unique_ptr<StorageDriver> make_gdrive_driver();
DriverConfigInfo gdrive_driver_info();
std::unique_ptr<StorageDriver> make_onedrive_driver();
DriverConfigInfo onedrive_driver_info();
std::unique_ptr<StorageDriver> make_dropbox_driver();
DriverConfigInfo dropbox_driver_info();
std::unique_ptr<StorageDriver> make_sftp_driver();
DriverConfigInfo sftp_driver_info();
std::unique_ptr<StorageDriver> make_ftp_driver();
DriverConfigInfo ftp_driver_info();

// ============================================================================
// 1. URLTree Driver (Zero-Storage Virtual HTTP/HTTPS Manifest Tree)
// ============================================================================
class UrlTreeDriver final : public StorageDriver {
public:
    static DriverConfigInfo static_info() {
        return DriverConfigInfo{
            .name = "URLTree",
            .display_name = "URL-Tree Virtual Index (HTTP/HTTPS Direct Links)",
            .local_sort = true,
            .no_cache = false,
            .no_upload = true,
            .default_proxy = false,
            .fields = {
                {"url_structure", "URL Manifest (path|url|size per line)", "text",
                 "/readme.txt|https://example.com/readme.txt|1024", "", true,
                 "One entry per line: /virtual/path/file.ext|https://target/url|size_bytes"},
            }
        };
    }
    DriverConfigInfo info() const override { return static_info(); }

    bool init(const StorageRecord& record, VfsEngine* vfs, std::string& err_out) override {
        auto j = JsonValue::parse(record.addition);
        if (!j || !j->is_object()) { err_out = "Invalid URLTree JSON"; return false; }
        web_proxy_ = record.web_proxy;
        allow_private_net_ = vfs && vfs->config().allow_private_net_outbound;
        entries_.clear();

        std::string text = j->get("url_structure").as_str();
        for (auto line : split_sv(text, '\n', true)) {
            line = trim_ascii(line);
            if (line.empty() || line.front() == '#') continue;
            auto cols = split_sv(line, '|', false);
            if (cols.size() < 2) continue;
            auto vp = clean_vpath(trim_ascii(cols[0]));
            std::string u(trim_ascii(cols[1]));
            if (!vp || *vp == "/" || (!u.starts_with("http://") && !u.starts_with("https://"))) continue;
            int64_t sz = (cols.size() >= 3) ? parse_i64(cols[2]).value_or(0) : 0;
            entries_[*vp] = Node{u, sz};
        }
        return true;
    }

    std::optional<FileObj> stat(std::string_view subpath, std::string& err_out) override {
        auto clean = clean_vpath(subpath).value_or("/");
        if (clean == "/") {
            FileObj r; r.name = "/"; r.is_dir = true; r.type = 1; r.modified = unix_now();
            return r;
        }
        if (auto it = entries_.find(clean); it != entries_.end()) {
            auto [_, base] = split_vpath(clean);
            FileObj f;
            f.name = base;
            f.is_dir = false;
            f.size = it->second.size;
            f.modified = unix_now();
            f.type = classify_file_type(base, false);
            return f;
        }
        // Check virtual folder prefix
        std::string pref = clean + "/";
        for (const auto& [k, _] : entries_) {
            if (k.starts_with(pref)) {
                auto [_, base] = split_vpath(clean);
                FileObj d; d.name = base; d.is_dir = true; d.type = 1; d.modified = unix_now();
                return d;
            }
        }
        err_out = "URLTree entry not found";
        return std::nullopt;
    }

    bool list(std::string_view subpath, std::vector<FileObj>& out, std::string& err_out) override {
        (void)err_out;
        auto clean = clean_vpath(subpath).value_or("/");
        std::string pref = (clean == "/") ? "/" : (clean + "/");
        std::map<std::string, FileObj> uniq;
        for (const auto& [k, node] : entries_) {
            if (!k.starts_with(pref) || k.size() <= pref.size()) continue;
            std::string_view rem = std::string_view(k).substr(pref.size());
            auto slash = rem.find('/');
            if (slash == std::string_view::npos) {
                FileObj f;
                f.name = std::string(rem);
                f.is_dir = false;
                f.size = node.size;
                f.modified = unix_now();
                f.type = classify_file_type(f.name, false);
                uniq[f.name] = std::move(f);
            } else {
                std::string dname(rem.substr(0, slash));
                if (!uniq.contains(dname)) {
                    FileObj d;
                    d.name = dname;
                    d.is_dir = true;
                    d.type = 1;
                    d.modified = unix_now();
                    uniq[dname] = std::move(d);
                }
            }
        }
        out.clear();
        for (auto& [_, v] : uniq) out.push_back(std::move(v));
        return true;
    }

    std::optional<DownloadResolution> resolve_download(
        std::string_view subpath,
        const std::map<std::string, std::string>&,
        std::string& err_out) override {
        auto clean = clean_vpath(subpath).value_or("/");
        auto it = entries_.find(clean);
        if (it == entries_.end()) { err_out = "URLTree file not found"; return std::nullopt; }
        if (!validate_outbound_url(it->second.url, allow_private_net_, &err_out)) {
            return std::nullopt; // SSRF guard!
        }
        auto [_, base] = split_vpath(clean);
        DownloadResolution res;
        res.mode = web_proxy_ ? DownloadResolution::Mode::UpstreamHttp : DownloadResolution::Mode::RedirectUrl;
        res.url = it->second.url;
        res.allow_private_net = allow_private_net_;
        res.file_meta.name = base;
        res.file_meta.size = it->second.size;
        res.mime_type = std::string(guess_mime_type(base));
        return res;
    }

private:
    struct Node { std::string url; int64_t size = 0; };
    std::map<std::string, Node> entries_;
    bool web_proxy_ = false;
    bool allow_private_net_ = false;
};

// ============================================================================
// 2. Alias Driver (Zero-Overhead VFS Path Remapping / Read-Only Projection)
// ============================================================================
class AliasDriver final : public StorageDriver {
public:
    static DriverConfigInfo static_info() {
        return DriverConfigInfo{
            .name = "Alias",
            .display_name = "Alias Virtual Mount (Remap Any VFS Path)",
            .local_sort = true,
            .no_cache = true,
            .no_upload = false,
            .default_proxy = true,
            .fields = {
                {"target_path", "Target VFS Path", "string", "/local", "", true, "Existing mounted VFS path to project"},
                {"read_only",   "Read Only",       "bool",   "true",   "", false, "Protect underlying storage from modifications"},
            }
        };
    }
    DriverConfigInfo info() const override { return static_info(); }

    bool init(const StorageRecord& record, VfsEngine* vfs, std::string& err_out) override {
        vfs_ = vfs;
        mount_self_ = record.mount_path;
        auto j = JsonValue::parse(record.addition);
        if (!j || !j->is_object()) { err_out = "Invalid Alias JSON"; return false; }
        target_ = clean_vpath(j->get("target_path").as_sv()).value_or("");
        read_only_ = j->get("read_only").as_bool(true);
        if (target_.empty() || target_ == "/" || is_subpath(mount_self_, target_)) {
            err_out = "Invalid or cyclic Alias target_path";
            return false;
        }
        return true;
    }

    std::optional<FileObj> stat(std::string_view subpath, std::string& err_out) override {
        if (!vfs_) return std::nullopt;
        std::string full = join_vpath(target_, subpath).value_or(target_);
        auto r = vfs_->get_obj(nullptr, full, "", "", 1);
        if (!r.ok) { err_out = r.error; return std::nullopt; }
        return r.entry;
    }

    bool list(std::string_view subpath, std::vector<FileObj>& out, std::string& err_out) override {
        if (!vfs_) return false;
        std::string full = join_vpath(target_, subpath).value_or(target_);
        auto r = vfs_->list_dir(nullptr, full, "", false, 1);
        if (!r.ok) { err_out = r.error; return false; }
        out = std::move(r.entries);
        return true;
    }

    std::optional<DownloadResolution> resolve_download(
        std::string_view subpath,
        const std::map<std::string, std::string>& hdrs,
        std::string& err_out) override {
        if (!vfs_) return std::nullopt;
        std::string full = join_vpath(target_, subpath).value_or(target_);
        return vfs_->resolve_download(full, hdrs, err_out, 1);
    }

    bool mkdir(std::string_view subpath, std::string& err_out) override {
        if (read_only_ || !vfs_) { err_out = "Alias mount is read-only"; return false; }
        return vfs_->mkdir(join_vpath(target_, subpath).value_or(target_), err_out, 1);
    }

    bool remove(std::string_view subpath, std::string& err_out) override {
        if (read_only_ || !vfs_) { err_out = "Alias mount is read-only"; return false; }
        return vfs_->remove(join_vpath(target_, subpath).value_or(target_), err_out, 1);
    }

    bool put(std::string_view dst_subpath, UploadStream& stream, std::string& err_out) override {
        if (read_only_ || !vfs_) { err_out = "Alias mount is read-only"; return false; }
        return vfs_->put_stream(join_vpath(target_, dst_subpath).value_or(target_), stream, err_out, 1);
    }

private:
    VfsEngine* vfs_ = nullptr;
    std::string mount_self_;
    std::string target_;
    bool read_only_ = true;
};

// ============================================================================
// 3. Crypt Driver: Hardware AES-256-GCM Chunked Streaming Wrapper Driver
//    Supports O(1) size calculation + Random-Access HTTP Range Video Seeking!
// ============================================================================
class CryptDriver final : public StorageDriver {
public:
    static constexpr size_t kHeaderSize   = 24; // "ALCRv1\0\0" (8B) + file_salt (16B)
    static constexpr size_t kPlainChunk   = 65536;
    static constexpr size_t kTagSize      = 16;
    static constexpr size_t kCipherChunk  = kPlainChunk + kTagSize; // 65552 bytes
    static constexpr std::string_view kMagic = std::string_view("ALCRv1\0\0", 8);

    static DriverConfigInfo static_info() {
        return DriverConfigInfo{
            .name = "Crypt",
            .display_name = "Crypt (AES-256-GCM Streaming + Random-Access Seek)",
            .local_sort = true,
            .no_cache = true,
            .no_upload = false,
            .default_proxy = true,
            .fields = {
                {"target_path",       "Encrypted Backing VFS Path", "string",   "",      "", true,  "Underlying mounted VFS path storing ciphertext"},
                {"password",          "Master Encryption Password", "password", "",      "", true,  "Passphrase used to derive AES-256-GCM keys"},
                {"salt",              "Key Derivation Salt",        "password", "alist-crypt-v1", "", false, "Optional custom salt"},
                {"encrypt_filenames", "Encrypt Filenames",          "bool",     "true",  "", false, "Encrypt file & folder names deterministically"},
            }
        };
    }
    DriverConfigInfo info() const override { return static_info(); }

    bool init(const StorageRecord& record, VfsEngine* vfs, std::string& err_out) override {
        vfs_ = vfs;
        auto j = JsonValue::parse(record.addition);
        if (!j || !j->is_object()) { err_out = "Invalid Crypt JSON"; return false; }
        target_ = clean_vpath(j->get("target_path").as_sv()).value_or("");
        std::string pwd  = j->get("password").as_str();
        std::string salt = j->get("salt").as_str("alist-crypt-v1");
        enc_names_ = j->get("encrypt_filenames").as_bool(true);
        if (target_.empty() || target_ == "/" || is_subpath(record.mount_path, target_)) {
            err_out = "Invalid or cyclic Crypt target_path";
            return false;
        }
        if (pwd.size() < 6) {
            err_out = "Crypt password must be at least 6 characters";
            return false;
        }
        master_key_ = derive_key_sha256(pwd, "crypt-data:" + salt);
        name_key_   = derive_key_sha256(pwd, "crypt-name:" + salt);
        return true;
    }

    static int64_t encrypted_to_plain_size(int64_t enc_size) noexcept {
        if (enc_size < static_cast<int64_t>(kHeaderSize)) return 0;
        int64_t body = enc_size - static_cast<int64_t>(kHeaderSize);
        if (body == 0) return 0;
        int64_t full = body / static_cast<int64_t>(kCipherChunk);
        int64_t rem  = body % static_cast<int64_t>(kCipherChunk);
        if (rem > 0 && rem < static_cast<int64_t>(kTagSize)) return 0;
        return full * static_cast<int64_t>(kPlainChunk) + (rem > 0 ? (rem - static_cast<int64_t>(kTagSize)) : 0);
    }

    std::optional<FileObj> stat(std::string_view subpath, std::string& err_out) override {
        if (!vfs_) return std::nullopt;
        std::string backing = map_to_backing_path(subpath);
        auto r = vfs_->get_obj(nullptr, backing, "", "", 1);
        if (!r.ok) { err_out = r.error; return std::nullopt; }
        FileObj f = r.entry;
        auto [_, plain_base] = split_vpath(clean_vpath(subpath).value_or("/"));
        f.name = plain_base.empty() ? "/" : plain_base;
        if (!f.is_dir) f.size = encrypted_to_plain_size(f.size);
        f.type = classify_file_type(f.name, f.is_dir);
        return f;
    }

    bool list(std::string_view subpath, std::vector<FileObj>& out, std::string& err_out) override {
        if (!vfs_) return false;
        std::string backing = map_to_backing_path(subpath);
        auto r = vfs_->list_dir(nullptr, backing, "", false, 1);
        if (!r.ok) { err_out = r.error; return false; }
        out.clear();
        for (const auto& item : r.entries) {
            auto dec_name = decrypt_name(item.name);
            if (!dec_name) continue;
            FileObj f = item;
            f.name = *dec_name;
            if (!f.is_dir) f.size = encrypted_to_plain_size(f.size);
            f.type = classify_file_type(f.name, f.is_dir);
            out.push_back(std::move(f));
        }
        return true;
    }

    std::optional<DownloadResolution> resolve_download(
        std::string_view subpath,
        const std::map<std::string, std::string>& req_headers,
        std::string& err_out) override {
        if (!vfs_) return std::nullopt;
        std::string backing = map_to_backing_path(subpath);
        auto under = vfs_->resolve_download(backing, req_headers, err_out, 1);
        if (!under) return std::nullopt;

        auto st = stat(subpath, err_out);
        if (!st || st->is_dir) return std::nullopt;

        std::array<uint8_t, 32> mkey = master_key_;
        DownloadResolution under_copy = *under;

        DownloadResolution res;
        res.mode = DownloadResolution::Mode::CustomStream;
        res.file_meta = *st;
        res.mime_type = std::string(guess_mime_type(st->name));
        res.stream_fn = [mkey, under_copy](int64_t offset, int64_t length,
                                           const std::function<bool(const char*, size_t)>& sink) -> bool {
            if (length <= 0) return true;
            uint64_t first_chunk = static_cast<uint64_t>(offset) / kPlainChunk;
            uint64_t last_chunk  = static_cast<uint64_t>(offset + length - 1) / kPlainChunk;

            // Keep a single FD open for local backing files (zero repeated open() overhead)
            UniqueFd local_fd;
            if (under_copy.mode == DownloadResolution::Mode::LocalFile) {
                local_fd.reset(::open(under_copy.local_path.c_str(), O_RDONLY | O_CLOEXEC));
                if (!local_fd) return false;
            }

            auto read_backing_slice = [&](int64_t p_off, size_t p_len, std::string& dst) -> bool {
                dst.clear();
                if (local_fd) {
                    dst.resize(p_len);
                    size_t got = 0;
                    while (got < p_len) {
                        ssize_t n = ::pread(local_fd.get(), dst.data() + got, p_len - got, p_off + static_cast<off_t>(got));
                        if (n < 0) {
                            if (errno == EINTR) continue;
                            return false;
                        }
                        if (n == 0) break;
                        got += static_cast<size_t>(n);
                    }
                    dst.resize(got);
                    return true;
                }
                if (!under_copy.url.empty()) {
                    OutboundRequest r;
                    r.url = under_copy.url;
                    r.headers = under_copy.headers;
                    r.headers["Range"] = "bytes=" + std::to_string(p_off) + "-" + std::to_string(p_off + static_cast<int64_t>(p_len) - 1);
                    r.username = under_copy.username;
                    r.password = under_copy.password;
                    r.allow_private_net = under_copy.allow_private_net;
                    auto resp = HttpClient::instance().perform(r);
                    if (!resp.ok || (resp.status_code != 206 && resp.status_code != 200)) return false;
                    if (resp.status_code == 200) {
                        if (static_cast<uint64_t>(p_off) >= resp.body.size()) {
                            dst.clear();
                        } else {
                            dst = resp.body.substr(static_cast<size_t>(p_off), p_len);
                        }
                    } else {
                        dst = std::move(resp.body);
                    }
                    return true;
                }
                return false;
            };

            std::string hdr;
            if (!read_backing_slice(0, kHeaderSize, hdr) || hdr.size() != kHeaderSize ||
                std::string_view(hdr.data(), 8) != kMagic) {
                return false;
            }
            std::string_view salt_sv(hdr.data() + 8, 16);
            auto file_key = hmac_sha256_bytes(std::string_view(reinterpret_cast<const char*>(mkey.data()), 32), salt_sv);

            // Bounded O(1) RAM streaming: decrypt at most 32 chunks (~2 MiB) per window
            constexpr uint64_t kWindowChunks = 32;
            std::string enc_window;
            int64_t emitted = 0;

            for (uint64_t win_start = first_chunk; win_start <= last_chunk && emitted < length; win_start += kWindowChunks) {
                uint64_t win_end = std::min<uint64_t>(last_chunk, win_start + kWindowChunks - 1);
                int64_t phys_start = static_cast<int64_t>(kHeaderSize + win_start * kCipherChunk);
                size_t want_bytes  = static_cast<size_t>((win_end - win_start + 1) * kCipherChunk);
                if (!read_backing_slice(phys_start, want_bytes, enc_window) || enc_window.empty()) return false;

                size_t pos = 0;
                for (uint64_t cidx = win_start; cidx <= win_end && pos < enc_window.size() && emitted < length; ++cidx) {
                    size_t c_len = std::min<size_t>(kCipherChunk, enc_window.size() - pos);
                    std::array<uint8_t, 12> iv{};
                    std::memcpy(iv.data(), salt_sv.data(), 4);
                    for (int b = 7; b >= 0; --b) iv[4 + (7 - b)] = static_cast<uint8_t>((cidx >> (b * 8)) & 0xFF);
                    std::string aad_str = "chunk:" + std::to_string(cidx);

                    auto dec = aes256_gcm_decrypt_with_iv(file_key, iv,
                                                         std::string_view(enc_window.data() + pos, c_len), aad_str);
                    if (!dec) return false;
                    pos += c_len;

                    size_t slice_start = (cidx == first_chunk)
                                             ? static_cast<size_t>(offset % static_cast<int64_t>(kPlainChunk))
                                             : 0;
                    if (slice_start >= dec->size()) break;
                    size_t avail = dec->size() - slice_start;
                    size_t to_send = static_cast<size_t>(std::min<int64_t>(static_cast<int64_t>(avail), length - emitted));
                    if (to_send > 0) {
                        if (!sink(dec->data() + slice_start, to_send)) return false;
                        emitted += static_cast<int64_t>(to_send);
                    }
                }
            }
            return emitted == length;
        };
        return res;
    }

    bool mkdir(std::string_view subpath, std::string& err_out) override {
        if (!vfs_) return false;
        return vfs_->mkdir(map_to_backing_path(subpath), err_out, 1);
    }

    bool remove(std::string_view subpath, std::string& err_out) override {
        if (!vfs_) return false;
        return vfs_->remove(map_to_backing_path(subpath), err_out, 1);
    }

    bool put(std::string_view dst_subpath, UploadStream& plain_stream, std::string& err_out) override {
        if (!vfs_) return false;
        std::string backing = map_to_backing_path(dst_subpath);

        std::array<uint8_t, 16> salt{};
        random_bytes(salt.data(), salt.size());
        std::string_view salt_sv(reinterpret_cast<const char*>(salt.data()), salt.size());
        auto file_key = hmac_sha256_bytes(std::string_view(reinterpret_cast<const char*>(master_key_.data()), 32), salt_sv);

        // Encrypt on-the-fly chunk-by-chunk with zero full-file RAM buffering!
        std::string pending;
        pending.append(kMagic);
        pending.append(salt_sv);
        uint64_t chunk_idx = 0;
        bool eof_reached = false;

        UploadStream enc_stream;
        auto [_, enc_base] = split_vpath(backing);
        enc_stream.filename = enc_base;
        if (plain_stream.size >= 0) {
            if (plain_stream.size == 0) {
                enc_stream.size = static_cast<int64_t>(kHeaderSize);
            } else {
                int64_t num_chunks = (plain_stream.size + static_cast<int64_t>(kPlainChunk) - 1) / static_cast<int64_t>(kPlainChunk);
                enc_stream.size = static_cast<int64_t>(kHeaderSize) + plain_stream.size + num_chunks * static_cast<int64_t>(kTagSize);
            }
        } else {
            enc_stream.size = -1;
        }

        enc_stream.read_fn = [&](char* out_buf, size_t max_len) -> ssize_t {
            while (pending.empty() && !eof_reached) {
                std::string plain_chunk(kPlainChunk, '\0');
                size_t got = 0;
                while (got < kPlainChunk) {
                    ssize_t n = plain_stream.read_fn(plain_chunk.data() + got, kPlainChunk - got);
                    if (n < 0) return -1;
                    if (n == 0) { eof_reached = true; break; }
                    got += static_cast<size_t>(n);
                }
                plain_chunk.resize(got);
                if (got == 0) break;

                std::array<uint8_t, 12> iv{};
                std::memcpy(iv.data(), salt.data(), 4);
                for (int b = 7; b >= 0; --b) iv[4 + (7 - b)] = static_cast<uint8_t>((chunk_idx >> (b * 8)) & 0xFF);
                std::string aad_str = "chunk:" + std::to_string(chunk_idx);
                auto ct = aes256_gcm_encrypt_with_iv(file_key, iv, plain_chunk, aad_str);
                if (!ct) return -1;
                pending.append(*ct);
                ++chunk_idx;
            }
            if (pending.empty()) return 0;
            size_t n = std::min(max_len, pending.size());
            std::memcpy(out_buf, pending.data(), n);
            pending.erase(0, n);
            return static_cast<ssize_t>(n);
        };

        bool ok = vfs_->put_stream(backing, enc_stream, err_out, 1);
        OPENSSL_cleanse(file_key.data(), file_key.size());
        return ok;
    }

private:
    VfsEngine* vfs_ = nullptr;
    std::string target_;
    std::array<uint8_t, 32> master_key_{};
    std::array<uint8_t, 32> name_key_{};
    bool enc_names_ = true;

    // Deterministic synthetic-IV AES-256-GCM filename encryption so path lookups are O(1)!
    std::string encrypt_name(std::string_view plain_name) const {
        if (!enc_names_) return std::string(plain_name);
        auto mac = hmac_sha256_bytes(std::string_view(reinterpret_cast<const char*>(name_key_.data()), 32), plain_name);
        std::array<uint8_t, 12> siv{};
        std::memcpy(siv.data(), mac.data(), 12);
        auto ct = aes256_gcm_encrypt_with_iv(name_key_, siv, plain_name, "alist-fname-v1");
        std::string raw(reinterpret_cast<const char*>(siv.data()), 12);
        raw.append(*ct);
        return "c1_" + base64url_encode(raw);
    }

    std::optional<std::string> decrypt_name(std::string_view stored_name) const {
        if (!enc_names_) return std::string(stored_name);
        if (!stored_name.starts_with("c1_")) return std::nullopt;
        auto raw = base64url_decode(stored_name.substr(3));
        if (!raw || raw->size() < 28) return std::nullopt;
        std::array<uint8_t, 12> siv{};
        std::memcpy(siv.data(), raw->data(), 12);
        return aes256_gcm_decrypt_with_iv(name_key_, siv, std::string_view(*raw).substr(12), "alist-fname-v1");
    }

    std::string map_to_backing_path(std::string_view subpath) const {
        auto clean = clean_vpath(subpath).value_or("/");
        if (clean == "/") return target_;
        std::string out = target_;
        for (auto seg : split_sv(clean, '/', true)) {
            out.push_back('/');
            out.append(encrypt_name(seg));
        }
        return out;
    }
};

// ============================================================================
// Global Driver Registry Implementation
// ============================================================================
std::vector<DriverConfigInfo> get_registered_drivers() {
    return {
        local_driver_info(),
        s3_driver_info(),
        webdav_driver_info(),
        gdrive_driver_info(),
        onedrive_driver_info(),
        dropbox_driver_info(),
        sftp_driver_info(),
        ftp_driver_info(),
        UrlTreeDriver::static_info(),
        AliasDriver::static_info(),
        CryptDriver::static_info(),
    };
}

std::unique_ptr<StorageDriver> create_driver_instance(std::string_view name) {
    if (name == "Local")       return make_local_driver();
    if (name == "S3")          return make_s3_driver();
    if (name == "WebDAV")      return make_webdav_driver();
    if (name == "GoogleDrive") return make_gdrive_driver();
    if (name == "OneDrive")    return make_onedrive_driver();
    if (name == "Dropbox")     return make_dropbox_driver();
    if (name == "SFTP")        return make_sftp_driver();
    if (name == "FTP")         return make_ftp_driver();
    if (name == "URLTree")     return std::make_unique<UrlTreeDriver>();
    if (name == "Alias")       return std::make_unique<AliasDriver>();
    if (name == "Crypt")       return std::make_unique<CryptDriver>();
    return nullptr;
}

} // namespace alist
