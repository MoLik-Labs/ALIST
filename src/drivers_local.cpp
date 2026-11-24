#include "alist/common.hpp"
#include "alist/crypto.hpp"
#include "alist/driver.hpp"
#include "alist/json.hpp"
#include "alist/security.hpp"

#include <cerrno>
#include <climits>
#include <cstdio>
#include <dirent.h>
#include <fcntl.h>
#include <filesystem>
#include <sys/stat.h>
#include <unistd.h>

namespace alist {

class LocalDriver final : public StorageDriver {
public:
    static DriverConfigInfo static_info() {
        return DriverConfigInfo{
            .name = "Local",
            .display_name = "Local Filesystem (Zero-Copy sendfile)",
            .local_sort = true,
            .no_cache = false,
            .no_upload = false,
            .default_proxy = true,
            .fields = {
                {"root_folder_path", "Root Folder Path", "string", "", "", true, "Absolute or relative local directory to mount"},
                {"show_hidden",      "Show Hidden Files", "bool",  "false", "", false, "Show dotfiles starting with '.'"},
                {"follow_symlinks",  "Allow Confined Symlinks", "bool", "false", "", false, "Allow symlinks whose targets stay strictly inside Root Folder"},
                {"read_only",        "Read Only",         "bool",  "false", "", false, "Block all write/delete/modify operations"},
            }
        };
    }

    DriverConfigInfo info() const override { return static_info(); }

    bool init(const StorageRecord& record, VfsEngine*, std::string& err_out) override {
        auto j = JsonValue::parse(record.addition);
        if (!j || !j->is_object()) {
            err_out = "Invalid Local driver addition JSON";
            return false;
        }
        std::string raw_root = j->get("root_folder_path").as_str();
        if (raw_root.empty()) {
            err_out = "root_folder_path is required";
            return false;
        }
        show_hidden_     = j->get("show_hidden").as_bool(false);
        follow_symlinks_ = j->get("follow_symlinks").as_bool(false);
        read_only_       = j->get("read_only").as_bool(false);

        std::error_code ec;
        if (!std::filesystem::exists(raw_root, ec)) {
            std::filesystem::create_directories(raw_root, ec);
        }
        char resolved[PATH_MAX]{};
        if (!::realpath(raw_root.c_str(), resolved)) {
            err_out = "Failed to resolve root_folder_path: " + raw_root;
            return false;
        }
        struct stat st{};
        if (::stat(resolved, &st) != 0 || !S_ISDIR(st.st_mode)) {
            err_out = "root_folder_path is not an accessible directory";
            return false;
        }
        root_realpath_ = resolved;
        return true;
    }

    std::optional<FileObj> stat(std::string_view subpath, std::string& err_out) override {
        auto full = resolve_confined_local_path(root_realpath_, subpath, follow_symlinks_);
        if (!full) {
            err_out = "Path denied or escapes storage root";
            return std::nullopt;
        }
        struct stat st{};
        int rc = follow_symlinks_ ? ::stat(full->c_str(), &st) : ::lstat(full->c_str(), &st);
        if (rc != 0) {
            err_out = "Object not found";
            return std::nullopt;
        }
        if (S_ISLNK(st.st_mode) && !follow_symlinks_) {
            err_out = "Symbolic links are disabled on this storage";
            return std::nullopt;
        }
        auto [_, base] = split_vpath(subpath);
        if (!show_hidden_ && !base.empty() && base.front() == '.') {
            err_out = "Object not found";
            return std::nullopt;
        }
        return stat_to_obj(base.empty() ? "/" : base, st, std::string(subpath));
    }

    bool list(std::string_view subpath, std::vector<FileObj>& out, std::string& err_out) override {
        auto full = resolve_confined_local_path(root_realpath_, subpath, follow_symlinks_);
        if (!full) {
            err_out = "Path denied or escapes storage root";
            return false;
        }
        DIR* dir = ::opendir(full->c_str());
        if (!dir) {
            err_out = "Unable to open directory";
            return false;
        }
        ScopeExit close_dir([&] { ::closedir(dir); });

        out.clear();
        while (dirent* ent = ::readdir(dir)) {
            std::string_view name(ent->d_name);
            if (name == "." || name == "..") continue;
            if (!show_hidden_ && !name.empty() && name.front() == '.') continue;

            std::string child_full = *full + "/" + std::string(name);
            struct stat st{};
            if (::lstat(child_full.c_str(), &st) != 0) continue;
            if (S_ISLNK(st.st_mode)) {
                if (!follow_symlinks_) continue;
                char link_real[PATH_MAX]{};
                if (!::realpath(child_full.c_str(), link_real) || !is_subpath(root_realpath_, link_real)) {
                    continue; // Skip symlinks pointing outside root!
                }
                if (::stat(link_real, &st) != 0) continue;
            }
            auto child_sub = join_vpath(subpath, name).value_or("/" + std::string(name));
            out.push_back(stat_to_obj(std::string(name), st, std::move(child_sub)));
        }
        return true;
    }

    std::optional<DownloadResolution> resolve_download(
        std::string_view subpath,
        const std::map<std::string, std::string>&,
        std::string& err_out) override {

        auto full = resolve_confined_local_path(root_realpath_, subpath, follow_symlinks_);
        if (!full) {
            err_out = "Path denied or escapes storage root";
            return std::nullopt;
        }
        struct stat st{};
        int rc = follow_symlinks_ ? ::stat(full->c_str(), &st) : ::lstat(full->c_str(), &st);
        if (rc != 0 || !S_ISREG(st.st_mode)) {
            err_out = "File not found";
            return std::nullopt;
        }
        auto [_, base] = split_vpath(subpath);
        if (!show_hidden_ && !base.empty() && base.front() == '.') {
            err_out = "File not found";
            return std::nullopt;
        }
        DownloadResolution res;
        res.mode       = DownloadResolution::Mode::LocalFile;
        res.local_path = *full;
        res.file_meta  = stat_to_obj(base, st, std::string(subpath));
        res.mime_type  = std::string(guess_mime_type(base));
        return res;
    }

    bool mkdir(std::string_view subpath, std::string& err_out) override {
        if (read_only_) { err_out = "Storage is read-only"; return false; }
        auto full = resolve_confined_local_path(root_realpath_, subpath, follow_symlinks_);
        if (!full || *full == root_realpath_) { err_out = "Invalid directory path"; return false; }
        std::error_code ec;
        std::filesystem::create_directories(*full, ec);
        if (ec) { err_out = ec.message(); return false; }
        return true;
    }

    bool remove(std::string_view subpath, std::string& err_out) override {
        if (read_only_) { err_out = "Storage is read-only"; return false; }
        auto full = resolve_confined_local_path(root_realpath_, subpath, follow_symlinks_);
        if (!full || *full == root_realpath_) { err_out = "Cannot remove storage root"; return false; }
        std::error_code ec;
        auto removed = std::filesystem::remove_all(*full, ec);
        if (ec || removed == 0) {
            err_out = ec ? ec.message() : "Target does not exist";
            return false;
        }
        return true;
    }

    bool rename_or_move(std::string_view src_subpath, std::string_view dst_subpath, std::string& err_out) override {
        if (read_only_) { err_out = "Storage is read-only"; return false; }
        auto src_full = resolve_confined_local_path(root_realpath_, src_subpath, follow_symlinks_);
        auto dst_full = resolve_confined_local_path(root_realpath_, dst_subpath, follow_symlinks_);
        if (!src_full || !dst_full || *src_full == root_realpath_ || *dst_full == root_realpath_) {
            err_out = "Invalid source or destination path";
            return false;
        }
        if (is_subpath(*src_full, *dst_full) && *src_full != *dst_full) {
            err_out = "Cannot move a directory into itself";
            return false;
        }
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(*dst_full).parent_path(), ec);
        std::filesystem::rename(*src_full, *dst_full, ec);
        if (ec) { err_out = ec.message(); return false; }
        return true;
    }

    bool copy(std::string_view src_subpath, std::string_view dst_subpath, std::string& err_out) override {
        if (read_only_) { err_out = "Storage is read-only"; return false; }
        auto src_full = resolve_confined_local_path(root_realpath_, src_subpath, follow_symlinks_);
        auto dst_full = resolve_confined_local_path(root_realpath_, dst_subpath, follow_symlinks_);
        if (!src_full || !dst_full) { err_out = "Invalid path"; return false; }
        if (is_subpath(*src_full, *dst_full)) {
            err_out = "Cannot copy a directory into itself";
            return false;
        }
        std::error_code ec;
        std::filesystem::create_directories(std::filesystem::path(*dst_full).parent_path(), ec);
        // Never blindly follow nested symlinks during recursive copy (prevents out-of-root leak)
        auto opts = std::filesystem::copy_options::overwrite_existing |
                    std::filesystem::copy_options::recursive |
                    std::filesystem::copy_options::skip_symlinks;
        std::filesystem::copy(*src_full, *dst_full, opts, ec);
        if (ec) { err_out = ec.message(); return false; }
        return true;
    }

    bool put(std::string_view dst_subpath, UploadStream& stream, std::string& err_out) override {
        if (read_only_) { err_out = "Storage is read-only"; return false; }
        auto dst_full = resolve_confined_local_path(root_realpath_, dst_subpath, follow_symlinks_);
        if (!dst_full || *dst_full == root_realpath_) {
            err_out = "Invalid upload target path";
            return false;
        }
        std::error_code ec;
        auto parent = std::filesystem::path(*dst_full).parent_path();
        std::filesystem::create_directories(parent, ec);
        if (ec) { err_out = ec.message(); return false; }

        // Atomic upload: write to unique hidden temp file in same directory, fsync/close, then atomic rename!
        std::string tmp_path = parent.string() + "/.alist_upload_" + random_hex(8) + ".part";
        int flags = O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC;
#ifdef O_NOFOLLOW
        flags |= O_NOFOLLOW;
#endif
        UniqueFd fd(::open(tmp_path.c_str(), flags, 0644));
        if (!fd) {
            err_out = "Failed to open temporary upload file";
            return false;
        }
        ScopeExit unlink_tmp([&] { ::unlink(tmp_path.c_str()); });

        std::vector<char> buf(131072); // 128 KiB write buffer
        int64_t remaining = stream.size; // -1 denotes unknown length
        while (true) {
            ssize_t n = stream.read_fn ? stream.read_fn(buf.data(), buf.size()) : 0;
            if (n < 0) {
                err_out = "Stream read error during upload";
                return false;
            }
            if (n == 0) {
                if (remaining > 0) { err_out = "Upload ended before the declared size"; return false; }
                break;
            }
            if (static_cast<size_t>(n) > buf.size() || (remaining >= 0 && n > remaining)) {
                err_out = "Upload exceeded the declared size or buffer";
                return false;
            }
            if (remaining >= 0) remaining -= n;
            size_t written = 0;
            while (written < static_cast<size_t>(n)) {
                ssize_t w = ::write(fd.get(), buf.data() + written, static_cast<size_t>(n) - written);
                if (w < 0 && errno == EINTR) continue;
                if (w <= 0) {
                    err_out = "Disk write failed during upload";
                    return false;
                }
                written += static_cast<size_t>(w);
            }
        }
        // Flush file data before publishing it. This detects delayed I/O errors;
        // directory metadata is not fsynced, so this is not a crash-durability guarantee.
        int sync_rc;
        do { sync_rc = ::fsync(fd.get()); } while (sync_rc < 0 && errno == EINTR);
        if (sync_rc < 0) { err_out = "Failed to sync uploaded file"; return false; }
        if (::close(fd.release()) != 0) { err_out = "Failed to close uploaded file"; return false; }

        if (::rename(tmp_path.c_str(), dst_full->c_str()) != 0) {
            err_out = "Atomic rename failed after upload";
            return false;
        }
        unlink_tmp.dismiss();
        return true;
    }

private:
    std::string root_realpath_;
    bool show_hidden_     = false;
    bool follow_symlinks_ = false;
    bool read_only_       = false;

    static FileObj stat_to_obj(std::string name, const struct stat& st, std::string actual) {
        FileObj o;
        o.name        = std::move(name);
        o.is_dir      = S_ISDIR(st.st_mode);
        o.size        = o.is_dir ? 0 : static_cast<int64_t>(st.st_size);
        o.modified    = static_cast<int64_t>(st.st_mtime);
        o.created     = static_cast<int64_t>(st.st_ctime);
        o.type        = classify_file_type(o.name, o.is_dir);
        o.actual_path = std::move(actual);
        char etag_buf[64];
        std::snprintf(etag_buf, sizeof(etag_buf), "\"%lx-%lx\"",
                      static_cast<unsigned long>(st.st_mtime),
                      static_cast<unsigned long>(st.st_size));
        o.etag = etag_buf;
        return o;
    }
};

std::unique_ptr<StorageDriver> make_local_driver() { return std::make_unique<LocalDriver>(); }
DriverConfigInfo local_driver_info() { return LocalDriver::static_info(); }

} // namespace alist
