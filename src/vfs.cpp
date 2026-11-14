#include "alist/vfs.hpp"
#include "alist/common.hpp"
#include "alist/crypto.hpp"
#include "alist/http_client.hpp"
#include "alist/logger.hpp"
#include "alist/security.hpp"

#include <algorithm>
#include <fcntl.h>
#include <set>
#include <unordered_set>
#include <unistd.h>

namespace alist {

namespace {
constexpr int kMaxVfsRecursion = 6;

thread_local int g_tl_vfs_depth = 0;

struct VfsDepthGuard {
    int depth;
    VfsDepthGuard() noexcept : depth(++g_tl_vfs_depth) {}
    ~VfsDepthGuard() noexcept { --g_tl_vfs_depth; }
};

bool is_balance_variant_of(std::string_view candidate_mount, std::string_view primary_mount) noexcept {
    if (candidate_mount.size() <= primary_mount.size() + 8) return false;
    return candidate_mount.starts_with(primary_mount) &&
           candidate_mount.substr(primary_mount.size(), 8) == ".balance";
}

void sort_entries(std::vector<FileObj>& entries, std::string_view order_by, std::string_view order_dir, std::string_view extract_folder) {
    const bool desc = iequals_ascii(order_dir, "DESC");
    int folder_mode = 1; // 1 = front (default), 2 = back, 0 = mixed
    if (iequals_ascii(extract_folder, "back")) folder_mode = 2;
    else if (iequals_ascii(extract_folder, "none")) folder_mode = 0;
    enum class SortKey { Name, Size, Modified };
    const SortKey sort_key = iequals_ascii(order_by, "size") ? SortKey::Size :
                             iequals_ascii(order_by, "modified") ? SortKey::Modified : SortKey::Name;

    std::sort(entries.begin(), entries.end(), [desc, folder_mode, sort_key](const FileObj& a, const FileObj& b) {
        if (folder_mode != 0 && a.is_dir != b.is_dir) {
            return (folder_mode == 1) ? a.is_dir : !a.is_dir;
        }
        int cmp = 0;
        if (sort_key == SortKey::Size) {
            cmp = (a.size < b.size) ? -1 : (a.size > b.size ? 1 : 0);
        } else if (sort_key == SortKey::Modified) {
            cmp = (a.modified < b.modified) ? -1 : (a.modified > b.modified ? 1 : 0);
        }
        if (cmp == 0) {
            cmp = a.name.compare(b.name);
        }
        return desc ? (cmp > 0) : (cmp < 0);
    });
}
} // namespace

VfsEngine::VfsEngine(Database& db, const AppConfig& cfg) : db_(db), cfg_(cfg) {}

void VfsEngine::reload_all_storages() {
    auto records = db_.list_storages(true);
    std::map<std::string, std::shared_ptr<MountedStorage>> fresh;
    for (auto& rec : records) {
        auto drv = create_driver_instance(rec.driver);
        if (!drv) {
            db_.update_storage_status(rec.id, "disabled: unknown driver " + rec.driver);
            continue;
        }
        std::string err;
        if (!drv->init(rec, this, err)) {
            LOG_WARN("vfs", "Storage init failed [" + rec.mount_path + "]: " + err);
            db_.update_storage_status(rec.id, "error: " + err);
            continue;
        }
        rec.status = "work";
        db_.update_storage_status(rec.id, "work");
        auto ms = std::make_shared<MountedStorage>();
        ms->record = std::move(rec);
        ms->driver = std::shared_ptr<StorageDriver>(std::move(drv));
        fresh[ms->record.mount_path] = std::move(ms);
    }
    {
        std::unique_lock<std::shared_mutex> lk(mounts_mu_);
        mounts_ = std::move(fresh);
    }
    list_cache_.clear();
}

bool VfsEngine::reload_storage(int64_t storage_id, std::string* err_out) {
    auto rec_opt = db_.get_storage(storage_id);
    if (!rec_opt) {
        if (err_out) *err_out = "Storage record not found";
        return false;
    }
    if (rec_opt->disabled) {
        unload_storage_path(rec_opt->mount_path);
        db_.update_storage_status(storage_id, "disabled");
        return true;
    }
    auto drv = create_driver_instance(rec_opt->driver);
    if (!drv) {
        std::string e = "Unknown driver: " + rec_opt->driver;
        db_.update_storage_status(storage_id, e);
        if (err_out) *err_out = std::move(e);
        return false;
    }
    std::string err;
    if (!drv->init(*rec_opt, this, err)) {
        db_.update_storage_status(storage_id, "error: " + err);
        if (err_out) *err_out = std::move(err);
        return false;
    }
    rec_opt->status = "work";
    db_.update_storage_status(storage_id, "work");

    auto ms = std::make_shared<MountedStorage>();
    ms->record = *rec_opt;
    ms->driver = std::shared_ptr<StorageDriver>(std::move(drv));
    {
        std::unique_lock<std::shared_mutex> lk(mounts_mu_);
        mounts_[ms->record.mount_path] = std::move(ms);
    }
    invalidate_cache(rec_opt->mount_path);
    return true;
}

void VfsEngine::unload_storage_path(std::string_view mount_path) {
    auto clean = clean_vpath(mount_path).value_or("");
    {
        std::unique_lock<std::shared_mutex> lk(mounts_mu_);
        mounts_.erase(clean);
    }
    invalidate_cache(clean);
    db_.remove_index_prefix(clean);
}

void VfsEngine::invalidate_cache(std::string_view global_vpath) {
    auto clean = clean_vpath(global_vpath).value_or("/");
    list_cache_.erase_prefix(clean);
    auto [par, _] = split_vpath(clean);
    list_cache_.erase(par);
}

std::optional<VfsEngine::MountMatch> VfsEngine::match_mount(std::string_view global_vpath) {
    auto clean = clean_vpath(global_vpath).value_or("/");
    std::shared_lock<std::shared_mutex> lk(mounts_mu_);

    std::shared_ptr<MountedStorage> best;
    for (const auto& [mp, ms] : mounts_) {
        if (mp.find(".balance") != std::string::npos) continue;
        if (is_subpath(mp, clean)) {
            if (!best || mp.size() > best->record.mount_path.size()) {
                best = ms;
            }
        }
    }
    if (!best) return std::nullopt;

    // Collect any .balance replicas for round-robin load distribution!
    std::vector<std::shared_ptr<MountedStorage>> candidates = {best};
    for (const auto& [mp, ms] : mounts_) {
        if (is_balance_variant_of(mp, best->record.mount_path)) {
            candidates.push_back(ms);
        }
    }
    std::string sub = relative_subpath(best->record.mount_path, clean);
    if (candidates.size() > 1) {
        uint64_t idx = balance_rr_.fetch_add(1, std::memory_order_relaxed) % candidates.size();
        return MountMatch{candidates[idx], std::move(sub)};
    }
    return MountMatch{best, std::move(sub)};
}

std::vector<std::string> VfsEngine::list_virtual_children(std::string_view global_vpath) {
    auto clean = clean_vpath(global_vpath).value_or("/");
    std::set<std::string> names;
    std::shared_lock<std::shared_mutex> lk(mounts_mu_);
    for (const auto& [mp, _] : mounts_) {
        if (mp.find(".balance") != std::string::npos) continue;
        if (mp == clean || !is_subpath(clean, mp)) continue;
        std::string rel = relative_subpath(clean, mp); // starts with '/'
        auto segs = split_sv(rel, '/', true);
        if (!segs.empty()) {
            names.emplace(segs.front());
        }
    }
    return std::vector<std::string>(names.begin(), names.end());
}

std::optional<std::string> VfsEngine::resolve_user_path(const UserRecord& user, std::string_view req_path) const {
    auto clean_req = clean_vpath(req_path);
    if (!clean_req) return std::nullopt;
    auto base = clean_vpath(user.base_path).value_or("/");
    auto joined = join_vpath(base, *clean_req);
    if (!joined || !is_subpath(base, *joined)) return std::nullopt;
    return joined;
}

int32_t VfsEngine::effective_permissions(const UserRecord& user, std::string_view vpath) {
    if (user.is_admin()) return kAllPermissionsMask;
    auto clean = clean_vpath(vpath).value_or("/");
    if (!is_subpath(user.base_path, clean)) return 0;

    int32_t perm = 0;
    for (int64_t rid : user.role_ids) {
        if (auto r = db_.get_role(rid)) {
            for (const auto& sc : r->scopes) {
                if (is_subpath(sc.path, clean)) {
                    perm |= sc.permission;
                }
            }
        }
    }
    if (user.role_ids.empty()) {
        perm |= user.legacy_permission;
    }
    return perm;
}

bool VfsEngine::is_path_in_role_scope(const UserRecord& user, std::string_view vpath) {
    if (user.is_admin()) return true;
    auto clean = clean_vpath(vpath).value_or("/");
    if (!is_subpath(user.base_path, clean)) return false;
    if (user.role_ids.empty()) return true;
    for (int64_t rid : user.role_ids) {
        if (auto r = db_.get_role(rid)) {
            for (const auto& sc : r->scopes) {
                if (is_subpath(sc.path, clean)) return true;
            }
        }
    }
    return false;
}

bool VfsEngine::can_read_path(const UserRecord& user, std::string_view vpath, std::string_view meta_password,
                              int* status_out, std::string* err_out) {
    auto clean = clean_vpath(vpath);
    if (!clean) {
        if (status_out) *status_out = 400;
        if (err_out) *err_out = "Illegal path";
        return false;
    }
    if (!user.is_admin()) {
        if (!is_subpath(user.base_path, *clean)) {
            if (status_out) *status_out = 403;
            if (err_out) *err_out = "Access outside user base_path is forbidden";
            return false;
        }
        // Check RBAC read visibility (either inside a scope OR ancestor directory leading to a scope)
        bool visible = user.role_ids.empty();
        for (int64_t rid : user.role_ids) {
            if (auto r = db_.get_role(rid)) {
                for (const auto& sc : r->scopes) {
                    if (is_subpath(sc.path, *clean) || is_subpath(*clean, sc.path)) {
                        visible = true;
                        break;
                    }
                }
            }
            if (visible) break;
        }
        if (!visible) {
            if (status_out) *status_out = 403;
            if (err_out) *err_out = "Permission denied by role scope";
            return false;
        }
    }

    int32_t perm = effective_permissions(user, *clean);
    if (auto meta = db_.find_nearest_meta(*clean)) {
        // 1. Check Hide pattern on target basename
        auto [parent_dir, basename] = split_vpath(*clean);
        if (!user.is_admin() && !has_perm(perm, PermSeeHides) && !basename.empty() && !meta->hide.empty()) {
            if (meta->path == parent_dir || (is_subpath(meta->path, parent_dir) && meta->h_sub)) {
                if (matches_any_hide_pattern(meta->hide, basename)) {
                    if (status_out) *status_out = 404;
                    if (err_out) *err_out = "Object not found";
                    return false;
                }
            }
        }
        // 2. Check Meta Password (constant-time comparison!)
        if (!user.is_admin() && !has_perm(perm, PermAccessWithoutPassword) && !meta->password.empty()) {
            if (meta->path == *clean || (is_subpath(meta->path, *clean) && meta->p_sub)) {
                if (!constant_time_equals(meta->password, meta_password)) {
                    if (status_out) *status_out = 403;
                    if (err_out) *err_out = "Password required or incorrect";
                    return false;
                }
            }
        }
    }
    return true;
}

bool VfsEngine::can_write_path(const UserRecord& user, std::string_view vpath, PermissionBit required_bit, std::string_view meta_password) {
    if (user.is_admin()) return true;
    auto clean = clean_vpath(vpath);
    if (!clean || !is_path_in_role_scope(user, *clean)) return false;
    if (!can_read_path(user, *clean, meta_password)) return false;

    int32_t perm = effective_permissions(user, *clean);
    if (has_perm(perm, required_bit)) return true;

    // Guest/User write via explicit Meta.write flag when PermWrite is checked
    if (required_bit == PermWrite) {
        if (auto meta = db_.find_nearest_meta(*clean)) {
            if (meta->write && (meta->path == *clean || meta->w_sub)) {
                return true;
            }
        }
    }
    return false;
}

bool VfsEngine::should_sign_path(std::string_view global_vpath) {
    if (db_.get_setting_bool("sign_all", false)) return true;
    if (auto mm = match_mount(global_vpath)) {
        return mm->mounted->record.enable_sign;
    }
    return false;
}

std::string VfsEngine::sign_vpath(std::string_view global_vpath) {
    auto clean = clean_vpath(global_vpath).value_or("/");
    std::string tok = db_.get_setting("token", cfg_.jwt_secret);
    int64_t exp_hours = db_.get_setting_int("link_expiration", 0);
    int64_t exp_ts = (exp_hours > 0) ? (unix_now() + exp_hours * 3600) : 0;
    return sign_url_payload(tok, clean, exp_ts);
}

bool VfsEngine::verify_vpath_signature(std::string_view global_vpath, std::string_view sign_param) {
    if (!should_sign_path(global_vpath)) return true;
    if (sign_param.empty()) return false;
    auto clean = clean_vpath(global_vpath).value_or("/");
    std::string tok = db_.get_setting("token", cfg_.jwt_secret);
    return verify_url_payload(tok, clean, sign_param);
}

VfsListResult VfsEngine::list_dir(const UserRecord* user, std::string_view global_vpath,
                                  std::string_view meta_password, bool refresh, int recursion_depth) {
    VfsDepthGuard depth_guard;
    const int eff_depth = std::max(recursion_depth, depth_guard.depth - 1);
    VfsListResult res;
    if (eff_depth > kMaxVfsRecursion) {
        res.http_status = 508;
        res.error = "Maximum virtual mount recursion depth exceeded";
        return res;
    }
    auto clean_opt = clean_vpath(global_vpath);
    if (!clean_opt) {
        res.http_status = 400;
        res.error = "Illegal path";
        return res;
    }
    const std::string& clean = *clean_opt;

    if (user && !can_read_path(*user, clean, meta_password, &res.http_status, &res.error)) {
        return res;
    }

    auto meta = db_.find_nearest_meta(clean);
    if (meta) {
        if (meta->path == clean || meta->r_sub) res.readme = meta->readme;
        if (meta->path == clean || meta->header_sub) res.header = meta->header;
        if (meta->write && (meta->path == clean || meta->w_sub)) res.write_allowed = true;
    }
    if (user && can_write_path(*user, clean, PermWrite, meta_password)) {
        res.write_allowed = true;
    }

    std::vector<FileObj> combined;
    auto mm = match_mount(clean);
    if (mm) {
        res.provider = mm->mounted->record.driver;
        const auto& srec = mm->mounted->record;
        auto drv_info = mm->mounted->driver->info();
        const std::string& cache_key = clean;

        ObjListSnapshot snap;
        if (!refresh && !drv_info.no_cache && srec.cache_expiration > 0) {
            if (auto cached = list_cache_.get(cache_key)) {
                snap = *cached;
            }
        }
        if (!snap) {
            auto load_fn = [&]() -> std::pair<ObjListSnapshot, std::string> {
                std::vector<FileObj> raw;
                std::string derr;
                if (!mm->mounted->driver->list(mm->subpath, raw, derr)) {
                    return {nullptr, derr};
                }
                if (drv_info.local_sort) {
                    sort_entries(raw, srec.order_by, srec.order_direction, srec.extract_folder);
                }
                auto ptr = std::make_shared<const std::vector<FileObj>>(std::move(raw));
                if (!drv_info.no_cache && srec.cache_expiration > 0) {
                    list_cache_.put(cache_key, ptr, std::chrono::seconds(srec.cache_expiration * 60));
                }
                return {ptr, ""};
            };
            auto [loaded_snap, err] = drv_info.no_cache
                                          ? load_fn()
                                          : list_singleflight_.execute(cache_key, load_fn);
            if (!loaded_snap) {
                res.http_status = 500;
                res.error = err.empty() ? "Failed to list directory" : err;
                return res;
            }
            snap = std::move(loaded_snap);
            if (eff_depth == 0 && db_.get_setting_bool("enable_search", true)) {
                db_.index_directory_entries(clean, *snap);
            }
        }
        combined.assign(snap->begin(), snap->end());
    }

    // Synthesize virtual child mount folders (e.g. "/" -> mounted storages).
    // A name set avoids rescanning every storage entry for each mounted child.
    auto vchildren = list_virtual_children(clean);
    std::unordered_set<std::string> existing_names;
    existing_names.reserve(combined.size() + vchildren.size());
    for (const auto& e : combined) existing_names.insert(e.name);
    for (const auto& vname : vchildren) {
        if (!existing_names.contains(vname)) {
            FileObj vf;
            vf.name = vname;
            vf.is_dir = true;
            vf.size = 0;
            vf.modified = unix_now();
            vf.type = 1;
            existing_names.insert(vname);
            combined.push_back(std::move(vf));
        }
    }

    if (!mm && vchildren.empty() && clean != "/") {
        res.http_status = 404;
        res.error = "Storage or directory not found";
        return res;
    }

    // Apply RBAC child scope visibility + Meta.Hide filtering + URL Signing
    bool need_sign = should_sign_path(clean);
    int32_t perm = user ? effective_permissions(*user, clean) : kAllPermissionsMask;
    bool check_hide = user && !user->is_admin() && !has_perm(perm, PermSeeHides) &&
                      meta.has_value() && !meta->hide.empty() &&
                      (meta->path == clean || (is_subpath(meta->path, clean) && meta->h_sub));

    std::vector<std::string> visible_scope_paths;
    if (user && !user->is_admin() && !user->role_ids.empty()) {
        for (int64_t rid : user->role_ids) {
            if (auto role = db_.get_role(rid)) {
                for (const auto& scope : role->scopes) visible_scope_paths.push_back(scope.path);
            }
        }
    }

    res.entries.reserve(combined.size());
    for (auto& item : combined) {
        auto child_vpath = join_vpath(clean, item.name).value_or(clean + "/" + item.name);
        if (check_hide && matches_any_hide_pattern(meta->hide, item.name)) {
            continue;
        }
        if (!visible_scope_paths.empty()) {
            bool child_visible = false;
            for (const auto& scope_path : visible_scope_paths) {
                if (is_subpath(scope_path, child_vpath) || is_subpath(child_vpath, scope_path)) {
                    child_visible = true;
                    break;
                }
            }
            if (!child_visible) continue;
        } else if (user && !user->is_admin() && !user->role_ids.empty()) {
            // A user assigned only to missing/empty roles has no scoped visibility.
            continue;
        }
        if (!item.is_dir && (need_sign || should_sign_path(child_vpath))) {
            item.sign = sign_vpath(child_vpath);
        }
        res.entries.push_back(std::move(item));
    }

    res.ok = true;
    return res;
}

VfsGetResult VfsEngine::get_obj(const UserRecord* user, std::string_view global_vpath,
                                std::string_view meta_password, std::string_view base_api_url,
                                int recursion_depth) {
    VfsDepthGuard depth_guard;
    VfsGetResult res;
    if (std::max(recursion_depth, depth_guard.depth - 1) > kMaxVfsRecursion) {
        res.http_status = 508;
        res.error = "Maximum virtual mount recursion depth exceeded";
        return res;
    }
    auto clean_opt = clean_vpath(global_vpath);
    if (!clean_opt) {
        res.http_status = 400;
        res.error = "Illegal path";
        return res;
    }
    const std::string& clean = *clean_opt;

    if (user && !can_read_path(*user, clean, meta_password, &res.http_status, &res.error)) {
        return res;
    }

    if (auto meta = db_.find_nearest_meta(clean)) {
        if (meta->path == clean || meta->r_sub) res.readme = meta->readme;
        if (meta->path == clean || meta->header_sub) res.header = meta->header;
    }

    auto mm = match_mount(clean);
    if (mm) {
        res.provider = mm->mounted->record.driver;
        std::string err;
        auto st = mm->mounted->driver->stat(mm->subpath, err);
        if (!st) {
            // Check if virtual intermediate directory
            if (!list_virtual_children(clean).empty()) {
                auto [_, base] = split_vpath(clean);
                res.entry.name = base.empty() ? "/" : base;
                res.entry.is_dir = true;
                res.entry.type = 1;
                res.entry.modified = unix_now();
                res.ok = true;
                return res;
            }
            res.http_status = 404;
            res.error = err.empty() ? "Object not found" : err;
            return res;
        }
        res.entry = *st;
        if (!res.entry.is_dir) {
            if (should_sign_path(clean)) {
                res.entry.sign = sign_vpath(clean);
            }
            std::string prefix = mm->mounted->record.web_proxy ? "/p" : "/d";
            res.raw_url = std::string(base_api_url) + prefix + url_encode(clean, true);
            if (!res.entry.sign.empty()) {
                res.raw_url += "?sign=" + url_encode(res.entry.sign);
            }
        }
        res.ok = true;
        return res;
    }

    if (clean == "/" || !list_virtual_children(clean).empty()) {
        auto [_, base] = split_vpath(clean);
        res.entry.name = base.empty() ? "/" : base;
        res.entry.is_dir = true;
        res.entry.type = 1;
        res.entry.modified = unix_now();
        res.ok = true;
        return res;
    }

    res.http_status = 404;
    res.error = "Object not found";
    return res;
}

std::optional<DownloadResolution> VfsEngine::resolve_download(
    std::string_view global_vpath,
    const std::map<std::string, std::string>& req_headers,
    std::string& err_out,
    int recursion_depth) {
    VfsDepthGuard depth_guard;
    if (std::max(recursion_depth, depth_guard.depth - 1) > kMaxVfsRecursion) {
        err_out = "Virtual mount recursion limit exceeded";
        return std::nullopt;
    }
    auto clean = clean_vpath(global_vpath);
    if (!clean) { err_out = "Illegal path"; return std::nullopt; }
    auto mm = match_mount(*clean);
    if (!mm) { err_out = "No storage mounted at path"; return std::nullopt; }
    return mm->mounted->driver->resolve_download(mm->subpath, req_headers, err_out);
}

bool VfsEngine::mkdir(std::string_view global_vpath, std::string& err_out, int recursion_depth) {
    if (recursion_depth > kMaxVfsRecursion) { err_out = "Recursion limit"; return false; }
    auto clean = clean_vpath(global_vpath);
    if (!clean) { err_out = "Illegal path"; return false; }
    auto mm = match_mount(*clean);
    if (!mm) { err_out = "Cannot create directory in unmounted virtual root"; return false; }
    if (!mm->mounted->driver->mkdir(mm->subpath, err_out)) return false;
    invalidate_cache(*clean);
    return true;
}

bool VfsEngine::remove(std::string_view global_vpath, std::string& err_out, int recursion_depth) {
    if (recursion_depth > kMaxVfsRecursion) { err_out = "Recursion limit"; return false; }
    auto clean = clean_vpath(global_vpath);
    if (!clean) { err_out = "Illegal path"; return false; }
    auto mm = match_mount(*clean);
    if (!mm || mm->subpath == "/") { err_out = "Cannot remove storage mount root"; return false; }
    if (!mm->mounted->driver->remove(mm->subpath, err_out)) return false;
    invalidate_cache(*clean);
    db_.remove_index_prefix(*clean);
    return true;
}

bool VfsEngine::rename_obj(std::string_view src_global_vpath, std::string_view new_name, std::string& err_out, int recursion_depth) {
    if (recursion_depth > kMaxVfsRecursion) { err_out = "Recursion limit"; return false; }
    if (!is_valid_filename(new_name)) { err_out = "Invalid target filename"; return false; }
    auto src_clean = clean_vpath(src_global_vpath);
    if (!src_clean) { err_out = "Illegal source path"; return false; }
    auto [parent, _] = split_vpath(*src_clean);
    auto dst_clean = join_vpath(parent, new_name);
    if (!dst_clean) { err_out = "Illegal destination path"; return false; }

    auto src_m = match_mount(*src_clean);
    auto dst_m = match_mount(*dst_clean);
    if (!src_m || !dst_m || src_m->mounted->record.id != dst_m->mounted->record.id || src_m->subpath == "/") {
        err_out = "Cannot rename mount root";
        return false;
    }
    if (!src_m->mounted->driver->rename_or_move(src_m->subpath, dst_m->subpath, err_out)) return false;
    invalidate_cache(*src_clean);
    invalidate_cache(*dst_clean);
    db_.remove_index_prefix(*src_clean);
    return true;
}

bool VfsEngine::copy_obj_sync(std::string_view src_global_vpath, std::string_view dst_dir_global_vpath, std::string& err_out, int recursion_depth) {
    if (recursion_depth > kMaxVfsRecursion + 8) { err_out = "Directory nesting depth exceeded"; return false; }
    auto src_clean = clean_vpath(src_global_vpath);
    auto dst_dir_clean = clean_vpath(dst_dir_global_vpath);
    if (!src_clean || !dst_dir_clean) { err_out = "Illegal path"; return false; }
    auto [_, src_name] = split_vpath(*src_clean);
    auto dst_full = join_vpath(*dst_dir_clean, src_name);
    if (!dst_full) { err_out = "Illegal destination path"; return false; }
    if (is_subpath(*src_clean, *dst_full)) {
        err_out = "Cannot copy an object into itself";
        return false;
    }

    auto src_m = match_mount(*src_clean);
    auto dst_m = match_mount(*dst_full);
    if (!src_m || !dst_m) { err_out = "Source or destination storage is not mounted"; return false; }

    // Fast path: Same storage native server-side copy!
    if (src_m->mounted->record.id == dst_m->mounted->record.id) {
        if (src_m->mounted->driver->copy(src_m->subpath, dst_m->subpath, err_out)) {
            invalidate_cache(*dst_full);
            return true;
        }
    }

    // Universal Cross-Storage Streaming Copy (works between ANY two drivers: Local <-> S3 <-> Crypt <-> WebDAV!)
    auto src_info = get_obj(nullptr, *src_clean, "", "", recursion_depth + 1);
    if (!src_info.ok) { err_out = src_info.error; return false; }

    if (src_info.entry.is_dir) {
        if (!mkdir(*dst_full, err_out, recursion_depth + 1)) return false;
        auto ls = list_dir(nullptr, *src_clean, "", true, recursion_depth + 1);
        if (!ls.ok) { err_out = ls.error; return false; }
        for (const auto& child : ls.entries) {
            auto child_src = join_vpath(*src_clean, child.name).value_or("");
            if (!copy_obj_sync(child_src, *dst_full, err_out, recursion_depth + 1)) return false;
        }
        return true;
    }

    auto dl = resolve_download(*src_clean, {}, err_out, recursion_depth + 1);
    if (!dl) return false;

    if (dl->mode == DownloadResolution::Mode::LocalFile) {
        UniqueFd fd(::open(dl->local_path.c_str(), O_RDONLY | O_CLOEXEC));
        if (!fd) { err_out = "Failed to open source file"; return false; }
        UploadStream up;
        up.filename = src_name;
        up.size = dl->file_meta.size;
        up.mime_type = dl->mime_type;
        up.read_fn = [&](char* buf, size_t max_len) -> ssize_t {
            return ::read(fd.get(), buf, max_len);
        };
        return put_stream(*dst_full, up, err_out, recursion_depth + 1);
    }

    if (dl->mode == DownloadResolution::Mode::CustomStream && dl->stream_fn) {
        // Buffer chunks through temporary spool file in temp_dir for bounded RAM!
        std::string spool = cfg_.temp_dir + "/copy_" + random_hex(8) + ".tmp";
        UniqueFd wfd(::open(spool.c_str(), O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600));
        if (!wfd) { err_out = "Failed to create spool file"; return false; }
        ScopeExit rm_spool([&] { ::unlink(spool.c_str()); });
        bool ok = dl->stream_fn(0, dl->file_meta.size, [&](const char* d, size_t n) {
            return ::write(wfd.get(), d, n) == static_cast<ssize_t>(n);
        });
        if (!ok) { err_out = "Failed to read encrypted/custom source stream"; return false; }
        ::lseek(wfd.get(), 0, SEEK_SET);
        UploadStream up;
        up.filename = src_name;
        up.size = dl->file_meta.size;
        up.read_fn = [&](char* buf, size_t max_len) -> ssize_t { return ::read(wfd.get(), buf, max_len); };
        return put_stream(*dst_full, up, err_out, recursion_depth + 1);
    }

    // Remote URL/UpstreamHttp -> spool to temp_dir then stream upload
    std::string spool = cfg_.temp_dir + "/copy_" + random_hex(8) + ".tmp";
    UniqueFd wfd(::open(spool.c_str(), O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600));
    if (!wfd) { err_out = "Failed to create spool file"; return false; }
    ScopeExit rm_spool([&] { ::unlink(spool.c_str()); });

    OutboundRequest r;
    r.url = dl->url;
    r.headers = dl->headers;
    r.username = dl->username;
    r.password = dl->password;
    r.ssh_private_key_file = dl->ssh_key_file;
    r.allow_private_net = dl->allow_private_net;
    int64_t total_spooled = 0;
    r.on_data = [&](const char* d, size_t n) -> bool {
        total_spooled += static_cast<int64_t>(n);
        return ::write(wfd.get(), d, n) == static_cast<ssize_t>(n);
    };
    auto resp = HttpClient::instance().perform(r);
    if (!resp.ok) { err_out = resp.error; return false; }
    ::lseek(wfd.get(), 0, SEEK_SET);
    UploadStream up;
    up.filename = src_name;
    up.size = total_spooled;
    up.read_fn = [&](char* buf, size_t max_len) -> ssize_t { return ::read(wfd.get(), buf, max_len); };
    return put_stream(*dst_full, up, err_out, recursion_depth + 1);
}

bool VfsEngine::move_obj(std::string_view src_global_vpath, std::string_view dst_dir_global_vpath, std::string& err_out, int recursion_depth) {
    auto src_clean = clean_vpath(src_global_vpath);
    auto dst_dir_clean = clean_vpath(dst_dir_global_vpath);
    if (!src_clean || !dst_dir_clean) { err_out = "Illegal path"; return false; }
    auto [_, src_name] = split_vpath(*src_clean);
    auto dst_full = join_vpath(*dst_dir_clean, src_name);
    if (!dst_full || is_subpath(*src_clean, *dst_full)) {
        err_out = "Invalid move destination";
        return false;
    }
    auto src_m = match_mount(*src_clean);
    auto dst_m = match_mount(*dst_full);
    if (!src_m || !dst_m || src_m->subpath == "/") {
        err_out = "Cannot move mount root or unmounted path";
        return false;
    }
    if (src_m->mounted->record.id == dst_m->mounted->record.id) {
        if (src_m->mounted->driver->rename_or_move(src_m->subpath, dst_m->subpath, err_out)) {
            invalidate_cache(*src_clean);
            invalidate_cache(*dst_full);
            db_.remove_index_prefix(*src_clean);
            return true;
        }
    }
    // Cross-storage move = Copy + Remove
    if (!copy_obj_sync(*src_clean, *dst_dir_clean, err_out, recursion_depth)) return false;
    return remove(*src_clean, err_out, recursion_depth);
}

bool VfsEngine::put_stream(std::string_view dst_global_vpath, UploadStream& stream, std::string& err_out, int recursion_depth) {
    if (recursion_depth > kMaxVfsRecursion) { err_out = "Recursion limit"; return false; }
    auto clean = clean_vpath(dst_global_vpath);
    if (!clean) { err_out = "Illegal destination path"; return false; }
    auto mm = match_mount(*clean);
    if (!mm || mm->subpath == "/") {
        err_out = "Cannot upload directly to virtual root (mount a storage first)";
        return false;
    }
    if (!mm->mounted->driver->put(mm->subpath, stream, err_out)) return false;
    invalidate_cache(*clean);
    return true;
}

int64_t VfsEngine::rebuild_search_index(std::string_view root_vpath, int max_depth) {
    auto clean = clean_vpath(root_vpath).value_or("/");
    if (clean == "/") db_.clear_search_index();
    else db_.remove_index_prefix(clean);

    int64_t indexed = 0;
    std::function<void(const std::string&, int)> walk = [&](const std::string& cur, int depth) {
        if (depth > max_depth) return;
        auto ls = list_dir(nullptr, cur, "", true, 0);
        if (!ls.ok) return;
        indexed += static_cast<int64_t>(ls.entries.size());
        for (const auto& e : ls.entries) {
            if (e.is_dir) {
                if (auto next = join_vpath(cur, e.name)) {
                    walk(*next, depth + 1);
                }
            }
        }
    };
    walk(clean, 0);
    return indexed;
}

} // namespace alist
