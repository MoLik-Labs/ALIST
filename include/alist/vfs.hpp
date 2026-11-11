#pragma once

#include "alist/cache.hpp"
#include "alist/config.hpp"
#include "alist/db.hpp"
#include "alist/driver.hpp"
#include "alist/models.hpp"

#include <atomic>
#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace alist {

struct MountedStorage {
    StorageRecord record;
    std::shared_ptr<StorageDriver> driver;
};

struct VfsListResult {
    bool ok = false;
    int http_status = 200;
    std::string error;
    std::vector<FileObj> entries;
    std::string readme;
    std::string header;
    std::string provider = "Virtual";
    bool write_allowed = false;
};

struct VfsGetResult {
    bool ok = false;
    int http_status = 200;
    std::string error;
    FileObj entry;
    std::string raw_url;
    std::string readme;
    std::string header;
    std::string provider = "Virtual";
};

class VfsEngine {
public:
    VfsEngine(Database& db, const AppConfig& cfg);

    // Mount lifecycle
    void reload_all_storages();
    bool reload_storage(int64_t storage_id, std::string* err_out = nullptr);
    void unload_storage_path(std::string_view mount_path);

    // RBAC & Meta Access Control
    int32_t effective_permissions(const UserRecord& user, std::string_view vpath);
    bool is_path_in_role_scope(const UserRecord& user, std::string_view vpath);
    bool can_read_path(const UserRecord& user, std::string_view vpath, std::string_view meta_password, int* status_out = nullptr, std::string* err_out = nullptr);
    bool can_write_path(const UserRecord& user, std::string_view vpath, PermissionBit required_bit, std::string_view meta_password = "");

    // Resolve user's BasePath + relative request path -> canonical global VFS path
    std::optional<std::string> resolve_user_path(const UserRecord& user, std::string_view req_path) const;

    // Sign URL helper
    bool should_sign_path(std::string_view global_vpath);
    std::string sign_vpath(std::string_view global_vpath);
    bool verify_vpath_signature(std::string_view global_vpath, std::string_view sign_param);

    // High-level VFS Operations (accepts global canonical vpath; internal drivers like Alias/Crypt also call these)
    VfsListResult list_dir(const UserRecord* user, std::string_view global_vpath, std::string_view meta_password, bool refresh, int recursion_depth = 0);
    VfsGetResult get_obj(const UserRecord* user, std::string_view global_vpath, std::string_view meta_password, std::string_view base_api_url, int recursion_depth = 0);
    std::optional<DownloadResolution> resolve_download(std::string_view global_vpath,
                                                       const std::map<std::string, std::string>& req_headers,
                                                       std::string& err_out,
                                                       int recursion_depth = 0);

    bool mkdir(std::string_view global_vpath, std::string& err_out, int recursion_depth = 0);
    bool remove(std::string_view global_vpath, std::string& err_out, int recursion_depth = 0);
    bool rename_obj(std::string_view src_global_vpath, std::string_view new_name, std::string& err_out, int recursion_depth = 0);
    bool move_obj(std::string_view src_global_vpath, std::string_view dst_dir_global_vpath, std::string& err_out, int recursion_depth = 0);
    bool copy_obj_sync(std::string_view src_global_vpath, std::string_view dst_dir_global_vpath, std::string& err_out, int recursion_depth = 0);
    bool put_stream(std::string_view dst_global_vpath, UploadStream& stream, std::string& err_out, int recursion_depth = 0);

    // Full-text index rebuild walk
    int64_t rebuild_search_index(std::string_view root_vpath = "/", int max_depth = 6);
    void invalidate_cache(std::string_view global_vpath);

    const AppConfig& config() const noexcept { return cfg_; }
    Database& db() noexcept { return db_; }

    [[nodiscard]] uint64_t cache_hits() const noexcept { return list_cache_.hits(); }
    [[nodiscard]] uint64_t cache_misses() const noexcept { return list_cache_.misses(); }
    [[nodiscard]] size_t cache_size() const { return list_cache_.size(); }
    [[nodiscard]] size_t mounted_count() const {
        std::shared_lock<std::shared_mutex> lk(mounts_mu_);
        return mounts_.size();
    }

private:
    mutable std::shared_mutex mounts_mu_;
    struct MountMatch {
        std::shared_ptr<MountedStorage> mounted;
        std::string subpath; // Canonical path inside driver
    };

    std::optional<MountMatch> match_mount(std::string_view global_vpath);
    std::vector<std::string> list_virtual_children(std::string_view global_vpath);

    Database& db_;
    const AppConfig& cfg_;

    std::map<std::string, std::shared_ptr<MountedStorage>> mounts_; // mount_path -> MountedStorage
    std::atomic<uint64_t> balance_rr_{0};

    // Immutable copy-on-write directory listing cache -> zero reader/writer slice race!
    using ObjListSnapshot = std::shared_ptr<const std::vector<FileObj>>;
    ShardedLruCache<ObjListSnapshot, 64> list_cache_{256};
    Singleflight<std::pair<ObjListSnapshot, std::string>> list_singleflight_;
};

} // namespace alist
