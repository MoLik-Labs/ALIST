#pragma once

#include "alist/models.hpp"

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct sqlite3;

namespace alist {

struct SearchHit {
    std::string parent;
    std::string name;
    bool is_dir = false;
    int64_t size = 0;
    int type = 0;
};

class Database {
public:
    Database() = default;
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    bool init(const std::string& db_path, const std::string& master_secret, std::string* initial_admin_pwd_out = nullptr);
    void close();

    // Settings
    std::string get_setting(std::string_view key, std::string_view def = "");
    int64_t get_setting_int(std::string_view key, int64_t def = 0);
    bool get_setting_bool(std::string_view key, bool def = false);
    bool set_setting(std::string_view key, std::string_view value, std::string_view group = "site", bool is_public = false);
    std::map<std::string, std::string> list_settings(bool public_only = false);

    // Roles
    std::vector<RoleRecord> list_roles();
    std::optional<RoleRecord> get_role(int64_t id);
    int64_t create_role(const RoleRecord& role);
    bool update_role(const RoleRecord& role);
    bool delete_role(int64_t id);

    // Users
    std::vector<UserRecord> list_users();
    std::optional<UserRecord> get_user_by_id(int64_t id);
    std::optional<UserRecord> get_user_by_name(std::string_view username);
    std::optional<UserRecord> get_admin_user();
    std::optional<UserRecord> get_guest_user();
    int64_t create_user(const UserRecord& user);
    bool update_user(const UserRecord& user);
    bool update_user_password(int64_t user_id, std::string_view new_plain_password);
    bool update_user_otp(int64_t user_id, std::string_view otp_secret, int64_t last_step = -1);
    bool verify_and_advance_otp_step(int64_t user_id, int64_t matched_step);
    bool delete_user(int64_t id);

    // JWT Revocation & Device Sessions (Atomic SQLite transaction -> zero TOCTOU race!)
    bool revoke_token_jti(std::string_view jti, int64_t exp_unix);
    bool is_token_jti_revoked(std::string_view jti);
    bool check_or_touch_session(int64_t user_id, std::string_view device_key,
                                std::string_view ua, std::string_view ip,
                                bool is_login, std::string* err_out = nullptr);
    std::vector<SessionRecord> list_sessions(std::optional<int64_t> user_id = std::nullopt);
    bool evict_session(int64_t user_id, std::string_view device_key);

    // Storages (addition JSON is AES-256-GCM encrypted at rest!)
    std::vector<StorageRecord> list_storages(bool enabled_only = false);
    std::optional<StorageRecord> get_storage(int64_t id);
    int64_t create_storage(StorageRecord rec);
    bool update_storage(const StorageRecord& rec);
    bool update_storage_status(int64_t id, std::string_view status);
    bool delete_storage(int64_t id);

    // Metas
    std::vector<MetaRecord> list_metas();
    std::optional<MetaRecord> get_meta(int64_t id);
    std::optional<MetaRecord> find_nearest_meta(std::string_view clean_path);
    int64_t create_meta(const MetaRecord& meta);
    bool update_meta(const MetaRecord& meta);
    bool delete_meta(int64_t id);

    // Public Shares
    std::vector<ShareRecord> list_shares(std::optional<int64_t> creator_id = std::nullopt);
    std::optional<ShareRecord> get_share(std::string_view share_id);
    bool create_share(const ShareRecord& share);
    bool update_share(const ShareRecord& share);
    bool increment_share_counter(std::string_view share_id, bool is_download);
    bool delete_share(std::string_view share_id, std::optional<int64_t> restrict_creator = std::nullopt);

    // FTS5 Full-Text Search Index (Unicode + Diacritic-insensitive!)
    void index_directory_entries(std::string_view parent_path, const std::vector<FileObj>& entries);
    void remove_index_prefix(std::string_view path_prefix);
    void clear_search_index();
    int64_t count_search_index();
    std::pair<std::vector<SearchHit>, int64_t> search_files(
        std::string_view keywords,
        std::string_view scope_parent,
        int scope_mode, // 0=all, 1=folder only, 2=file only
        int page,
        int per_page);

private:
    sqlite3* db_ = nullptr;
    std::string master_secret_;
    std::recursive_mutex mu_;

    bool exec_sql(const char* sql);
    void seed_defaults(std::string* initial_admin_pwd_out);
};

} // namespace alist
