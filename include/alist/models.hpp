#pragma once

#include "alist/json.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace alist {

// Exact 17-bit RBAC permission flags compatible with AList v3 / OpenList
enum PermissionBit : uint32_t {
    PermSeeHides              = 0,
    PermAccessWithoutPassword = 1,
    PermAddOfflineDownload    = 2,
    PermWrite                 = 3,
    PermRename                = 4,
    PermMove                  = 5,
    PermCopy                  = 6,
    PermRemove                = 7,
    PermWebdavRead            = 8,
    PermWebdavManage          = 9,
    PermFTPAccess             = 10,
    PermFTPManage             = 11,
    PermReadArchives          = 12,
    PermDecompress            = 13,
    PermPathLimit             = 14,
    PermMCPAccess             = 15,
    PermMCPManage             = 16,
};

inline constexpr int32_t kAllPermissionsMask = (1 << 17) - 1;
inline constexpr int32_t kDefaultGuestPerms  = (1 << PermWebdavRead) | (1 << PermReadArchives);

inline constexpr bool has_perm(int32_t mask, PermissionBit bit) noexcept {
    return ((mask >> static_cast<uint32_t>(bit)) & 1) != 0;
}

struct FileObj {
    std::string name;
    int64_t size       = 0;
    bool is_dir        = false;
    int64_t modified   = 0;
    int64_t created    = 0;
    std::string sign;
    std::string thumb;
    int type           = 0;
    std::string hash_info;
    std::string etag;
    std::string id;          // Cloud provider object ID (GDrive fileId, OneDrive itemId, etc.)
    std::string actual_path; // Path inside storage

    [[nodiscard]] JsonValue to_json(bool include_id = false) const {
        JsonValue j = JsonValue::object({
            {"name",      name},
            {"size",      size},
            {"is_dir",    is_dir},
            {"modified",  format_iso8601_str()},
            {"created",   format_iso8601_created()},
            {"sign",      sign},
            {"thumb",     thumb},
            {"type",      type},
            {"hashinfo",  hash_info.empty() ? "null" : hash_info},
            {"hash_info", nullptr},
        });
        if (include_id && !id.empty()) j["id"] = id;
        return j;
    }

    [[nodiscard]] std::string format_iso8601_str() const;
    [[nodiscard]] std::string format_iso8601_created() const;
};

struct RoleScope {
    std::string path = "/";
    int32_t permission = 0;
};

struct RoleRecord {
    int64_t id = 0;
    std::string name;
    std::string description;
    bool default_inheritable = false;
    std::vector<RoleScope> scopes;
};

// User role constants: 0 = general user, 1 = guest, 2 = admin
inline constexpr int kUserGeneral = 0;
inline constexpr int kUserGuest   = 1;
inline constexpr int kUserAdmin   = 2;

struct UserRecord {
    int64_t id = 0;
    std::string username;
    std::string password_hash;
    int64_t pwd_ts = 0;
    std::string base_path = "/";
    int identity = kUserGeneral; // 0=general, 1=guest, 2=admin
    std::vector<int64_t> role_ids;
    int32_t legacy_permission = 0;
    bool disabled = false;
    std::string otp_secret;
    int64_t last_otp_step = -1; // Prevents TOTP replay within the same 30s window!

    [[nodiscard]] bool is_admin() const noexcept { return identity == kUserAdmin; }
    [[nodiscard]] bool is_guest() const noexcept { return identity == kUserGuest; }
};

struct StorageRecord {
    int64_t id = 0;
    std::string mount_path;
    int order_idx = 0;
    std::string driver;
    int cache_expiration = 30; // minutes
    std::string status = "work";
    std::string addition = "{}"; // Decrypted in memory; encrypted at rest in SQLite!
    std::string remark;
    int64_t modified = 0;
    bool disabled = false;
    bool enable_sign = false;
    std::string order_by = "name";
    std::string order_direction = "ASC";
    std::string extract_folder;
    bool web_proxy = false;
    std::string webdav_policy = "native_proxy"; // "302_redirect" or "native_proxy"
    std::string down_proxy_url;
};

struct MetaRecord {
    int64_t id = 0;
    std::string path = "/";
    std::string password;
    bool p_sub = false;
    bool write = false;
    bool w_sub = false;
    std::string hide;
    bool h_sub = false;
    std::string readme;
    bool r_sub = false;
    std::string header;
    bool header_sub = false;
};

struct ShareRecord {
    int64_t id = 0;
    std::string share_id;
    int64_t creator_id = 0;
    std::string creator_name;
    std::string path;
    bool is_dir = false;
    std::string password_hash;
    bool has_password = false;
    int64_t expires_at = 0; // 0 = never
    int64_t max_downloads = 0; // 0 = unlimited
    int64_t downloads = 0;
    int64_t views = 0;
    bool disabled = false;
    int64_t created_at = 0;
    int64_t updated_at = 0;
};

struct SessionRecord {
    int64_t user_id = 0;
    std::string device_key;
    std::string user_agent;
    std::string ip;
    int64_t last_active = 0;
    int status = 0; // 0 = active, 1 = evicted/inactive
};

struct TaskRecord {
    std::string id;
    std::string type;   // "copy", "offline_download", "upload"
    std::string name;
    std::string creator;
    std::string state;  // "pending", "running", "succeeded", "failed", "canceled"
    std::string status_text;
    double progress = 0.0;
    std::string error_msg;
    int64_t created_at = 0;
    int64_t updated_at = 0;
};

} // namespace alist
