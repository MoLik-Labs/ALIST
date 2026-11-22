#pragma once

#include "alist/models.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace alist {

class VfsEngine;

struct DriverField {
    std::string name;
    std::string label;
    std::string type = "string"; // "string", "password", "bool", "number", "select", "text"
    std::string default_val;
    std::string options;         // comma-separated for select
    bool required = false;
    std::string help_text;
};

struct DriverConfigInfo {
    std::string name;
    std::string display_name;
    bool local_sort     = true;
    bool no_cache       = false;
    bool no_upload      = false;
    bool default_proxy  = false;
    std::vector<DriverField> fields;
};

struct UploadStream {
    std::string filename;
    int64_t size = -1; // -1 if chunked/unknown
    std::string mime_type;
    // Pull chunks from incoming HTTP request or task file; returns 0 on EOF, <0 on error
    std::function<ssize_t(char* buf, size_t max_len)> read_fn;
    // Optional temp file path if already spooled to disk
    std::string temp_file_path;
};

struct DownloadResolution {
    enum class Mode {
        LocalFile,    // Kernel zero-copy sendfile64()
        RedirectUrl,  // 302 Direct URL (S3 presigned, OneDrive, Dropbox, URLTree)
        UpstreamHttp, // Proxy HTTP/SFTP/FTP URL via HttpClient with Range pass-through
        CustomStream  // Computed stream (e.g. AES-256-GCM random-access chunk decryption!)
    };

    Mode mode = Mode::LocalFile;
    std::string local_path;
    std::string url;
    std::map<std::string, std::string> headers;
    std::string username;
    std::string password;
    std::string ssh_key_file;
    bool allow_private_net = false;

    FileObj file_meta;
    std::string mime_type = "application/octet-stream";

    // For Mode::CustomStream: streams exact byte slice [offset, offset + length)
    std::function<bool(int64_t offset, int64_t length, const std::function<bool(const char*, size_t)>& sink)> stream_fn;
};

class StorageDriver {
public:
    virtual ~StorageDriver() = default;

    virtual DriverConfigInfo info() const = 0;
    virtual bool init(const StorageRecord& record, VfsEngine* vfs, std::string& err_out) = 0;

    // Path inside storage (always canonicalized, e.g. "/" or "/folder/file.txt")
    virtual std::optional<FileObj> stat(std::string_view subpath, std::string& err_out) = 0;
    virtual bool list(std::string_view subpath, std::vector<FileObj>& out, std::string& err_out) = 0;
    virtual std::optional<DownloadResolution> resolve_download(
        std::string_view subpath,
        const std::map<std::string, std::string>& req_headers,
        std::string& err_out) = 0;

    virtual bool mkdir(std::string_view subpath, std::string& err_out) {
        err_out = "mkdir is not supported by this storage driver";
        return false;
    }
    virtual bool remove(std::string_view subpath, std::string& err_out) {
        err_out = "remove is not supported by this storage driver";
        return false;
    }
    virtual bool rename_or_move(std::string_view src_subpath, std::string_view dst_subpath, std::string& err_out) {
        err_out = "move/rename is not supported by this storage driver";
        return false;
    }
    virtual bool copy(std::string_view src_subpath, std::string_view dst_subpath, std::string& err_out) {
        err_out = "native copy is not supported by this storage driver";
        return false;
    }
    virtual bool put(std::string_view dst_subpath, UploadStream& stream, std::string& err_out) {
        err_out = "upload is not supported by this storage driver";
        return false;
    }
};

// Global registry of built-in storage drivers
std::vector<DriverConfigInfo> get_registered_drivers();
std::unique_ptr<StorageDriver> create_driver_instance(std::string_view driver_name);

} // namespace alist
