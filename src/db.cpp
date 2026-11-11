#include "alist/db.hpp"
#include "alist/common.hpp"
#include "alist/crypto.hpp"
#include "alist/json.hpp"
#include "alist/logger.hpp"
#include "alist/security.hpp"
#include "alist/version.hpp"
#include "c_libs.hpp"

#include <algorithm>
#include <filesystem>
#include <sys/stat.h>

namespace alist {

namespace {

class Stmt {
public:
    Stmt(sqlite3* db, std::string_view sql) {
        sqlite3_prepare_v2(db, sql.data(), static_cast<int>(sql.size()), &stmt_, nullptr);
    }
    ~Stmt() {
        if (stmt_) sqlite3_finalize(stmt_);
    }
    Stmt(const Stmt&) = delete;
    Stmt& operator=(const Stmt&) = delete;

    sqlite3_stmt* get() const noexcept { return stmt_; }
    explicit operator bool() const noexcept { return stmt_ != nullptr; }

    void bind_i64(int idx, int64_t v) { sqlite3_bind_int64(stmt_, idx, v); }
    void bind_int(int idx, int v)     { sqlite3_bind_int(stmt_, idx, v); }
    void bind_str(int idx, std::string_view sv) {
        sqlite3_bind_text(stmt_, idx, sv.data(), static_cast<int>(sv.size()), SQLITE_TRANSIENT);
    }
    int step() { return sqlite3_step(stmt_); }

    int64_t col_i64(int c) const { return sqlite3_column_int64(stmt_, c); }
    int col_int(int c) const     { return sqlite3_column_int(stmt_, c); }
    std::string col_str(int c) const {
        const unsigned char* p = sqlite3_column_text(stmt_, c);
        int n = sqlite3_column_bytes(stmt_, c);
        if (!p || n <= 0) return "";
        return std::string(reinterpret_cast<const char*>(p), static_cast<size_t>(n));
    }

private:
    sqlite3_stmt* stmt_ = nullptr;
};

std::string serialize_role_ids(const std::vector<int64_t>& ids) {
    JsonValue arr = JsonValue::array();
    for (int64_t id : ids) arr.push_back(id);
    return arr.dump();
}

std::vector<int64_t> parse_role_ids(std::string_view s) {
    std::vector<int64_t> out;
    auto j = JsonValue::parse(s);
    if (j && j->is_array()) {
        for (const auto& v : j->as_array()) out.push_back(v.as_i64(0));
    }
    return out;
}

std::string serialize_scopes(const std::vector<RoleScope>& scopes) {
    JsonValue arr = JsonValue::array();
    for (const auto& sc : scopes) {
        arr.push_back(JsonValue::object({
            {"path",       sc.path},
            {"permission", static_cast<int64_t>(sc.permission)},
        }));
    }
    return arr.dump();
}

std::vector<RoleScope> parse_scopes(std::string_view s) {
    std::vector<RoleScope> out;
    auto j = JsonValue::parse(s);
    if (j && j->is_array()) {
        for (const auto& v : j->as_array()) {
            RoleScope sc;
            sc.path = clean_vpath(v.get("path").as_sv("/")).value_or("/");
            sc.permission = static_cast<int32_t>(v.get("permission").as_i64(0));
            out.push_back(std::move(sc));
        }
    }
    return out;
}

UserRecord row_to_user(const Stmt& st) {
    UserRecord u;
    u.id                = st.col_i64(0);
    u.username          = st.col_str(1);
    u.password_hash     = st.col_str(2);
    u.pwd_ts            = st.col_i64(3);
    u.base_path         = clean_vpath(st.col_str(4)).value_or("/");
    u.identity          = st.col_int(5);
    u.role_ids          = parse_role_ids(st.col_str(6));
    u.legacy_permission = st.col_int(7);
    u.disabled          = st.col_int(8) != 0;
    u.otp_secret        = st.col_str(9);
    u.last_otp_step     = st.col_i64(10);
    return u;
}

StorageRecord row_to_storage(const Stmt& st, std::string_view master_secret) {
    StorageRecord s;
    s.id               = st.col_i64(0);
    s.mount_path       = clean_vpath(st.col_str(1)).value_or("/");
    s.order_idx        = st.col_int(2);
    s.driver           = st.col_str(3);
    s.cache_expiration = st.col_int(4);
    s.status           = st.col_str(5);
    s.addition         = vault_decrypt(master_secret, st.col_str(6));
    s.remark           = st.col_str(7);
    s.modified         = st.col_i64(8);
    s.disabled         = st.col_int(9) != 0;
    s.enable_sign      = st.col_int(10) != 0;
    s.order_by         = st.col_str(11);
    s.order_direction  = st.col_str(12);
    s.extract_folder   = st.col_str(13);
    s.web_proxy        = st.col_int(14) != 0;
    s.webdav_policy    = st.col_str(15);
    s.down_proxy_url   = st.col_str(16);
    return s;
}

MetaRecord row_to_meta(const Stmt& st, std::string_view master_secret) {
    MetaRecord m;
    m.id         = st.col_i64(0);
    m.path       = clean_vpath(st.col_str(1)).value_or("/");
    m.password   = vault_decrypt(master_secret, st.col_str(2));
    m.p_sub      = st.col_int(3) != 0;
    m.write      = st.col_int(4) != 0;
    m.w_sub      = st.col_int(5) != 0;
    m.hide       = st.col_str(6);
    m.h_sub      = st.col_int(7) != 0;
    m.readme     = st.col_str(8);
    m.r_sub      = st.col_int(9) != 0;
    m.header     = st.col_str(10);
    m.header_sub = st.col_int(11) != 0;
    return m;
}

ShareRecord row_to_share(const Stmt& st) {
    ShareRecord s;
    s.id            = st.col_i64(0);
    s.share_id      = st.col_str(1);
    s.creator_id    = st.col_i64(2);
    s.creator_name  = st.col_str(3);
    s.path          = clean_vpath(st.col_str(4)).value_or("/");
    s.is_dir        = st.col_int(5) != 0;
    s.password_hash = st.col_str(6);
    s.has_password  = !s.password_hash.empty();
    s.expires_at    = st.col_i64(7);
    s.max_downloads = st.col_i64(8);
    s.downloads     = st.col_i64(9);
    s.views         = st.col_i64(10);
    s.disabled      = st.col_int(11) != 0;
    s.created_at    = st.col_i64(12);
    s.updated_at    = st.col_i64(13);
    return s;
}

std::string session_map_key(int64_t uid, std::string_view dev_key) {
    return std::to_string(uid) + ":" + std::string(dev_key);
}

} // namespace

Database::~Database() { close(); }

void Database::close() {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    if (db_) {
        sqlite3_close_v2(db_);
        db_ = nullptr;
    }
    std::unique_lock<std::shared_mutex> wlk(cache_mu_);
    settings_cache_.clear();
    roles_cache_.clear();
    role_idx_by_id_.clear();
    users_cache_.clear();
    user_idx_by_id_.clear();
    user_idx_by_name_.clear();
    metas_cache_.clear();
    revoked_jti_cache_.clear();
    session_cache_.clear();
}

int64_t Database::db_file_size_bytes() const {
    if (db_path_.empty() || db_path_ == ":memory:") return 0;
    std::error_code ec;
    int64_t total = 0;
    if (std::filesystem::exists(db_path_, ec)) {
        total += static_cast<int64_t>(std::filesystem::file_size(db_path_, ec));
    }
    if (std::filesystem::exists(db_path_ + "-wal", ec)) {
        total += static_cast<int64_t>(std::filesystem::file_size(db_path_ + "-wal", ec));
    }
    return total;
}

bool Database::exec_sql(const char* sql) {
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        LOG_ERROR("db", std::string("SQLite exec error: ") + (errmsg ? errmsg : "unknown"));
        if (errmsg) sqlite3_free(errmsg);
        return false;
    }
    return true;
}

void Database::reload_settings_cache_locked() {
    std::unordered_map<std::string, SettingEntry, TransparentStringHash, std::equal_to<>> next;
    Stmt st(db_, "SELECT key, value, is_public FROM x_settings;");
    while (st.step() == SQLITE_ROW) {
        next.emplace(st.col_str(0), SettingEntry{st.col_str(1), st.col_int(2) != 0});
    }
    std::unique_lock<std::shared_mutex> wlk(cache_mu_);
    settings_cache_ = std::move(next);
}

void Database::reload_roles_cache_locked() {
    std::vector<RoleRecord> next;
    std::unordered_map<int64_t, size_t> idx;
    Stmt st(db_, "SELECT id, name, description, default_inheritable, scopes_json FROM x_roles ORDER BY id ASC;");
    while (st.step() == SQLITE_ROW) {
        RoleRecord r;
        r.id = st.col_i64(0);
        r.name = st.col_str(1);
        r.description = st.col_str(2);
        r.default_inheritable = st.col_int(3) != 0;
        r.scopes = parse_scopes(st.col_str(4));
        idx[r.id] = next.size();
        next.push_back(std::move(r));
    }
    std::unique_lock<std::shared_mutex> wlk(cache_mu_);
    roles_cache_ = std::move(next);
    role_idx_by_id_ = std::move(idx);
}

void Database::reload_users_cache_locked() {
    std::vector<UserRecord> next;
    std::unordered_map<int64_t, size_t> by_id;
    std::unordered_map<std::string, size_t, TransparentStringHash, std::equal_to<>> by_name;
    Stmt st(db_, "SELECT id, username, password_hash, pwd_ts, base_path, identity, role_ids_json, "
                 "legacy_permission, disabled, otp_secret, last_otp_step FROM x_users ORDER BY id ASC;");
    while (st.step() == SQLITE_ROW) {
        auto u = row_to_user(st);
        u.otp_secret = vault_decrypt(master_secret_, u.otp_secret);
        size_t pos = next.size();
        by_id[u.id] = pos;
        by_name.emplace(u.username, pos);
        next.push_back(std::move(u));
    }
    std::unique_lock<std::shared_mutex> wlk(cache_mu_);
    users_cache_ = std::move(next);
    user_idx_by_id_ = std::move(by_id);
    user_idx_by_name_ = std::move(by_name);
}

void Database::reload_metas_cache_locked() {
    std::vector<MetaRecord> next;
    Stmt st(db_, "SELECT id, path, password, p_sub, write_allow, w_sub, hide, h_sub, readme, r_sub, header, header_sub "
                 "FROM x_metas ORDER BY LENGTH(path) DESC;");
    while (st.step() == SQLITE_ROW) {
        next.push_back(row_to_meta(st, master_secret_));
    }
    std::unique_lock<std::shared_mutex> wlk(cache_mu_);
    metas_cache_ = std::move(next);
}

void Database::reload_auth_caches_locked() {
    int64_t now = unix_now();
    std::unordered_map<std::string, int64_t, TransparentStringHash, std::equal_to<>> jtis;
    {
        Stmt st(db_, "SELECT jti, exp_unix FROM x_revoked_tokens WHERE exp_unix > ?1;");
        st.bind_i64(1, now);
        while (st.step() == SQLITE_ROW) {
            jtis.emplace(st.col_str(0), st.col_i64(1));
        }
    }
    std::unordered_map<std::string, SessionRecord, TransparentStringHash, std::equal_to<>> sess;
    {
        Stmt st(db_, "SELECT user_id, device_key, user_agent, ip, last_active, status FROM x_sessions;");
        while (st.step() == SQLITE_ROW) {
            SessionRecord s;
            s.user_id     = st.col_i64(0);
            s.device_key  = st.col_str(1);
            s.user_agent  = st.col_str(2);
            s.ip          = st.col_str(3);
            s.last_active = st.col_i64(4);
            s.status      = st.col_int(5);
            sess.emplace(session_map_key(s.user_id, s.device_key), std::move(s));
        }
    }
    std::unique_lock<std::shared_mutex> wlk(cache_mu_);
    revoked_jti_cache_ = std::move(jtis);
    session_cache_ = std::move(sess);
}

bool Database::init(const std::string& db_path, const std::string& master_secret, std::string* initial_admin_pwd_out) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    close();
    db_path_ = db_path;
    master_secret_ = master_secret;

    int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    if (sqlite3_open_v2(db_path.c_str(), &db_, flags, nullptr) != SQLITE_OK) {
        LOG_ERROR("db", "Failed to open SQLite database: " + db_path);
        return false;
    }
    if (db_path != ":memory:") {
        ::chmod(db_path.c_str(), 0600);
    }

    sqlite3_busy_timeout(db_, 5000);
    exec_sql("PRAGMA journal_mode = WAL;");
    exec_sql("PRAGMA synchronous = NORMAL;");
    exec_sql("PRAGMA foreign_keys = ON;");
    exec_sql("PRAGMA temp_store = MEMORY;");
    exec_sql("PRAGMA cache_size = -16000;");
    exec_sql("PRAGMA mmap_size = 134217728;");

    const char* schema_sql = R"SQL(
        CREATE TABLE IF NOT EXISTS x_settings (
            key TEXT PRIMARY KEY,
            value TEXT NOT NULL,
            grp TEXT NOT NULL DEFAULT 'site',
            is_public INTEGER NOT NULL DEFAULT 0
        );
        CREATE TABLE IF NOT EXISTS x_roles (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL UNIQUE,
            description TEXT NOT NULL DEFAULT '',
            default_inheritable INTEGER NOT NULL DEFAULT 0,
            scopes_json TEXT NOT NULL DEFAULT '[]'
        );
        CREATE TABLE IF NOT EXISTS x_users (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            username TEXT NOT NULL UNIQUE,
            password_hash TEXT NOT NULL,
            pwd_ts INTEGER NOT NULL,
            base_path TEXT NOT NULL DEFAULT '/',
            identity INTEGER NOT NULL DEFAULT 0,
            role_ids_json TEXT NOT NULL DEFAULT '[]',
            legacy_permission INTEGER NOT NULL DEFAULT 0,
            disabled INTEGER NOT NULL DEFAULT 0,
            otp_secret TEXT NOT NULL DEFAULT '',
            last_otp_step INTEGER NOT NULL DEFAULT -1
        );
        CREATE TABLE IF NOT EXISTS x_storages (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            mount_path TEXT NOT NULL UNIQUE,
            order_idx INTEGER NOT NULL DEFAULT 0,
            driver TEXT NOT NULL,
            cache_expiration INTEGER NOT NULL DEFAULT 30,
            status TEXT NOT NULL DEFAULT 'work',
            addition_enc TEXT NOT NULL DEFAULT '{}',
            remark TEXT NOT NULL DEFAULT '',
            modified INTEGER NOT NULL DEFAULT 0,
            disabled INTEGER NOT NULL DEFAULT 0,
            enable_sign INTEGER NOT NULL DEFAULT 0,
            order_by TEXT NOT NULL DEFAULT 'name',
            order_direction TEXT NOT NULL DEFAULT 'ASC',
            extract_folder TEXT NOT NULL DEFAULT '',
            web_proxy INTEGER NOT NULL DEFAULT 0,
            webdav_policy TEXT NOT NULL DEFAULT 'native_proxy',
            down_proxy_url TEXT NOT NULL DEFAULT ''
        );
        CREATE TABLE IF NOT EXISTS x_metas (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            path TEXT NOT NULL UNIQUE,
            password TEXT NOT NULL DEFAULT '',
            p_sub INTEGER NOT NULL DEFAULT 0,
            write_allow INTEGER NOT NULL DEFAULT 0,
            w_sub INTEGER NOT NULL DEFAULT 0,
            hide TEXT NOT NULL DEFAULT '',
            h_sub INTEGER NOT NULL DEFAULT 0,
            readme TEXT NOT NULL DEFAULT '',
            r_sub INTEGER NOT NULL DEFAULT 0,
            header TEXT NOT NULL DEFAULT '',
            header_sub INTEGER NOT NULL DEFAULT 0
        );
        CREATE TABLE IF NOT EXISTS x_shares (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            share_id TEXT NOT NULL UNIQUE,
            creator_id INTEGER NOT NULL,
            creator_name TEXT NOT NULL,
            path TEXT NOT NULL,
            is_dir INTEGER NOT NULL DEFAULT 0,
            password_hash TEXT NOT NULL DEFAULT '',
            expires_at INTEGER NOT NULL DEFAULT 0,
            max_downloads INTEGER NOT NULL DEFAULT 0,
            downloads INTEGER NOT NULL DEFAULT 0,
            views INTEGER NOT NULL DEFAULT 0,
            disabled INTEGER NOT NULL DEFAULT 0,
            created_at INTEGER NOT NULL,
            updated_at INTEGER NOT NULL
        );
        CREATE TABLE IF NOT EXISTS x_sessions (
            user_id INTEGER NOT NULL,
            device_key TEXT NOT NULL,
            user_agent TEXT NOT NULL DEFAULT '',
            ip TEXT NOT NULL DEFAULT '',
            last_active INTEGER NOT NULL,
            status INTEGER NOT NULL DEFAULT 0,
            PRIMARY KEY (user_id, device_key)
        );
        CREATE TABLE IF NOT EXISTS x_revoked_tokens (
            jti TEXT PRIMARY KEY,
            exp_unix INTEGER NOT NULL
        );
        CREATE VIRTUAL TABLE IF NOT EXISTS x_search_fts USING fts5(
            name,
            parent_path,
            full_path UNINDEXED,
            is_dir UNINDEXED,
            size UNINDEXED,
            file_type UNINDEXED,
            tokenize='unicode61 remove_diacritics 2'
        );
    )SQL";

    if (!exec_sql(schema_sql)) return false;
    seed_defaults(initial_admin_pwd_out);
    reload_settings_cache_locked();
    reload_roles_cache_locked();
    reload_users_cache_locked();
    reload_metas_cache_locked();
    reload_auth_caches_locked();
    return true;
}

void Database::seed_defaults(std::string* initial_admin_pwd_out) {
    auto ensure_setting = [&](std::string_view k, std::string_view v, std::string_view g, bool pub) {
        Stmt st(db_, "INSERT OR IGNORE INTO x_settings(key, value, grp, is_public) VALUES(?1, ?2, ?3, ?4);");
        st.bind_str(1, k);
        st.bind_str(2, v);
        st.bind_str(3, g);
        st.bind_int(4, pub ? 1 : 0);
        st.step();
    };

    ensure_setting("version", kVersion, "site", true);
    ensure_setting("site_title", "AList · @nggkhaiz", "site", true);
    ensure_setting("announcement", "", "site", true);
    ensure_setting("pagination_type", "all", "site", true);
    ensure_setting("default_page_size", "50", "site", true);
    ensure_setting("allow_indexed", "false", "site", true);
    ensure_setting("allow_mounted", "true", "site", true);
    ensure_setting("robots_txt", "User-agent: *\nDisallow: /", "site", true);
    ensure_setting("search_index", "fts5", "index", true);
    ensure_setting("enable_search", "true", "index", true);
    ensure_setting("sign_all", "false", "global", false);
    ensure_setting("link_expiration", "0", "global", false);
    ensure_setting("token", "nggkhaiz-" + random_hex(24), "global", false);
    ensure_setting("max_devices", "0", "session", false);
    ensure_setting("device_evict_policy", "evict_oldest", "session", false);
    ensure_setting("device_session_ttl", "86400", "session", false);
    ensure_setting("forward_direct_link_params", "false", "global", false);

    // Seed built-in Roles & Users if not present (query directly before cache reload)
    Stmt rc(db_, "SELECT COUNT(*) FROM x_roles;");
    int64_t rcount = (rc.step() == SQLITE_ROW) ? rc.col_i64(0) : 0;
    if (rcount == 0) {
        RoleRecord admin_role;
        admin_role.name = "admin";
        admin_role.description = "Full administrative access across all paths";
        admin_role.scopes = {{.path = "/", .permission = kAllPermissionsMask}};
        int64_t admin_rid = create_role(admin_role);

        RoleRecord guest_role;
        guest_role.name = "guest";
        guest_role.description = "Default read-only browsing access";
        guest_role.scopes = {{.path = "/", .permission = kDefaultGuestPerms}};
        int64_t guest_rid = create_role(guest_role);

        Stmt uc(db_, "SELECT COUNT(*) FROM x_users WHERE username = 'admin';");
        if (uc.step() == SQLITE_ROW && uc.col_i64(0) == 0) {
            std::string init_pwd = random_base64url(12);
            if (const char* env_pw = std::getenv("ALIST_ADMIN_PASSWORD"); env_pw && *env_pw) {
                init_pwd = env_pw;
            }
            if (initial_admin_pwd_out) *initial_admin_pwd_out = init_pwd;

            UserRecord admin_u;
            admin_u.username = "admin";
            admin_u.password_hash = hash_password_pbkdf2(init_pwd);
            admin_u.pwd_ts = unix_now();
            admin_u.base_path = "/";
            admin_u.identity = kUserAdmin;
            admin_u.role_ids = {admin_rid};
            admin_u.legacy_permission = kAllPermissionsMask;
            admin_u.disabled = false;
            create_user(admin_u);
        }

        Stmt gc(db_, "SELECT COUNT(*) FROM x_users WHERE username = 'guest';");
        if (gc.step() == SQLITE_ROW && gc.col_i64(0) == 0) {
            UserRecord guest_u;
            guest_u.username = "guest";
            guest_u.password_hash = "";
            guest_u.pwd_ts = unix_now();
            guest_u.base_path = "/";
            guest_u.identity = kUserGuest;
            guest_u.role_ids = {guest_rid};
            guest_u.legacy_permission = kDefaultGuestPerms;
            guest_u.disabled = true;
            create_user(guest_u);
        }
    }
}

std::string Database::get_setting(std::string_view key, std::string_view def) {
    std::shared_lock<std::shared_mutex> rlk(cache_mu_);
    auto it = settings_cache_.find(key);
    return (it != settings_cache_.end()) ? it->second.value : std::string(def);
}

int64_t Database::get_setting_int(std::string_view key, int64_t def) {
    std::shared_lock<std::shared_mutex> rlk(cache_mu_);
    auto it = settings_cache_.find(key);
    if (it == settings_cache_.end()) return def;
    return parse_i64(it->second.value).value_or(def);
}

bool Database::get_setting_bool(std::string_view key, bool def) {
    std::shared_lock<std::shared_mutex> rlk(cache_mu_);
    auto it = settings_cache_.find(key);
    if (it == settings_cache_.end()) return def;
    auto s = to_lower_ascii(it->second.value);
    if (s == "true" || s == "1" || s == "yes") return true;
    if (s == "false" || s == "0" || s == "no") return false;
    return def;
}

bool Database::set_setting(std::string_view key, std::string_view value, std::string_view group, bool is_public) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    Stmt st(db_, "INSERT INTO x_settings(key, value, grp, is_public) VALUES(?1, ?2, ?3, ?4) "
                 "ON CONFLICT(key) DO UPDATE SET value = excluded.value;");
    st.bind_str(1, key);
    st.bind_str(2, value);
    st.bind_str(3, group);
    st.bind_int(4, is_public ? 1 : 0);
    bool ok = (st.step() == SQLITE_DONE);
    if (ok) reload_settings_cache_locked();
    return ok;
}

std::map<std::string, std::string> Database::list_settings(bool public_only) {
    std::shared_lock<std::shared_mutex> rlk(cache_mu_);
    std::map<std::string, std::string> out;
    for (const auto& [k, e] : settings_cache_) {
        if (!public_only || e.is_public) out.emplace(k, e.value);
    }
    return out;
}

std::vector<RoleRecord> Database::list_roles() {
    std::shared_lock<std::shared_mutex> rlk(cache_mu_);
    return roles_cache_;
}

std::optional<RoleRecord> Database::get_role(int64_t id) {
    std::shared_lock<std::shared_mutex> rlk(cache_mu_);
    auto it = role_idx_by_id_.find(id);
    if (it == role_idx_by_id_.end()) return std::nullopt;
    return roles_cache_[it->second];
}

int64_t Database::create_role(const RoleRecord& role) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    Stmt st(db_, "INSERT INTO x_roles(name, description, default_inheritable, scopes_json) VALUES(?1, ?2, ?3, ?4);");
    st.bind_str(1, role.name);
    st.bind_str(2, role.description);
    st.bind_int(3, role.default_inheritable ? 1 : 0);
    st.bind_str(4, serialize_scopes(role.scopes));
    if (st.step() != SQLITE_DONE) return 0;
    int64_t id = sqlite3_last_insert_rowid(db_);
    reload_roles_cache_locked();
    return id;
}

bool Database::update_role(const RoleRecord& role) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    Stmt st(db_, "UPDATE x_roles SET name=?1, description=?2, default_inheritable=?3, scopes_json=?4 WHERE id=?5;");
    st.bind_str(1, role.name);
    st.bind_str(2, role.description);
    st.bind_int(3, role.default_inheritable ? 1 : 0);
    st.bind_str(4, serialize_scopes(role.scopes));
    st.bind_i64(5, role.id);
    bool ok = (st.step() == SQLITE_DONE && sqlite3_changes(db_) > 0);
    if (ok) reload_roles_cache_locked();
    return ok;
}

bool Database::delete_role(int64_t id) {
    if (id <= 2) return false;
    std::lock_guard<std::recursive_mutex> lk(mu_);
    Stmt st(db_, "DELETE FROM x_roles WHERE id = ?1;");
    st.bind_i64(1, id);
    bool ok = (st.step() == SQLITE_DONE && sqlite3_changes(db_) > 0);
    if (ok) reload_roles_cache_locked();
    return ok;
}

std::vector<UserRecord> Database::list_users() {
    std::shared_lock<std::shared_mutex> rlk(cache_mu_);
    return users_cache_;
}

std::optional<UserRecord> Database::get_user_by_id(int64_t id) {
    std::shared_lock<std::shared_mutex> rlk(cache_mu_);
    auto it = user_idx_by_id_.find(id);
    if (it == user_idx_by_id_.end()) return std::nullopt;
    return users_cache_[it->second];
}

std::optional<UserRecord> Database::get_user_by_name(std::string_view username) {
    std::shared_lock<std::shared_mutex> rlk(cache_mu_);
    auto it = user_idx_by_name_.find(username);
    if (it == user_idx_by_name_.end()) return std::nullopt;
    return users_cache_[it->second];
}

std::optional<UserRecord> Database::get_admin_user() {
    std::shared_lock<std::shared_mutex> rlk(cache_mu_);
    for (const auto& u : users_cache_) {
        if (u.identity == kUserAdmin) return u;
    }
    return std::nullopt;
}

std::optional<UserRecord> Database::get_guest_user() {
    std::shared_lock<std::shared_mutex> rlk(cache_mu_);
    for (const auto& u : users_cache_) {
        if (u.identity == kUserGuest) return u;
    }
    return std::nullopt;
}

int64_t Database::create_user(const UserRecord& user) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    Stmt st(db_, "INSERT INTO x_users(username, password_hash, pwd_ts, base_path, identity, role_ids_json, "
                 "legacy_permission, disabled, otp_secret, last_otp_step) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10);");
    st.bind_str(1, user.username);
    st.bind_str(2, user.password_hash);
    st.bind_i64(3, user.pwd_ts > 0 ? user.pwd_ts : unix_now());
    st.bind_str(4, clean_vpath(user.base_path).value_or("/"));
    st.bind_int(5, user.identity);
    st.bind_str(6, serialize_role_ids(user.role_ids));
    st.bind_int(7, user.legacy_permission);
    st.bind_int(8, user.disabled ? 1 : 0);
    st.bind_str(9, user.otp_secret.empty() ? "" : vault_encrypt(master_secret_, user.otp_secret));
    st.bind_i64(10, user.last_otp_step);
    if (st.step() != SQLITE_DONE) return 0;
    int64_t id = sqlite3_last_insert_rowid(db_);
    reload_users_cache_locked();
    return id;
}

bool Database::update_user(const UserRecord& user) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    Stmt st(db_, "UPDATE x_users SET username=?1, password_hash=?2, pwd_ts=?3, base_path=?4, identity=?5, "
                 "role_ids_json=?6, legacy_permission=?7, disabled=?8, otp_secret=?9 WHERE id=?10;");
    st.bind_str(1, user.username);
    st.bind_str(2, user.password_hash);
    st.bind_i64(3, user.pwd_ts);
    st.bind_str(4, clean_vpath(user.base_path).value_or("/"));
    st.bind_int(5, user.identity);
    st.bind_str(6, serialize_role_ids(user.role_ids));
    st.bind_int(7, user.legacy_permission);
    st.bind_int(8, user.disabled ? 1 : 0);
    st.bind_str(9, user.otp_secret.empty() ? "" : vault_encrypt(master_secret_, user.otp_secret));
    st.bind_i64(10, user.id);
    bool ok = (st.step() == SQLITE_DONE && sqlite3_changes(db_) > 0);
    if (ok) reload_users_cache_locked();
    return ok;
}

bool Database::update_user_password(int64_t user_id, std::string_view new_plain_password) {
    std::string h = hash_password_pbkdf2(new_plain_password);
    std::lock_guard<std::recursive_mutex> lk(mu_);
    int64_t ts = unix_now();
    Stmt get_ts(db_, "SELECT pwd_ts FROM x_users WHERE id = ?1;");
    get_ts.bind_i64(1, user_id);
    if (get_ts.step() == SQLITE_ROW) {
        int64_t prev = get_ts.col_i64(0);
        if (ts <= prev) ts = prev + 1;
    }
    Stmt st(db_, "UPDATE x_users SET password_hash=?1, pwd_ts=?2 WHERE id=?3;");
    st.bind_str(1, h);
    st.bind_i64(2, ts);
    st.bind_i64(3, user_id);
    bool ok = (st.step() == SQLITE_DONE && sqlite3_changes(db_) > 0);
    if (ok) reload_users_cache_locked();
    return ok;
}

bool Database::update_user_otp(int64_t user_id, std::string_view otp_secret, int64_t last_step) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    Stmt st(db_, "UPDATE x_users SET otp_secret=?1, last_otp_step=?2 WHERE id=?3;");
    st.bind_str(1, otp_secret.empty() ? "" : vault_encrypt(master_secret_, otp_secret));
    st.bind_i64(2, last_step);
    st.bind_i64(3, user_id);
    bool ok = (st.step() == SQLITE_DONE && sqlite3_changes(db_) > 0);
    if (ok) reload_users_cache_locked();
    return ok;
}

bool Database::verify_and_advance_otp_step(int64_t user_id, int64_t matched_step) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    Stmt st(db_, "UPDATE x_users SET last_otp_step = ?1 WHERE id = ?2 AND last_otp_step < ?1;");
    st.bind_i64(1, matched_step);
    st.bind_i64(2, user_id);
    bool ok = (st.step() == SQLITE_DONE && sqlite3_changes(db_) > 0);
    if (ok) {
        std::unique_lock<std::shared_mutex> wlk(cache_mu_);
        auto it = user_idx_by_id_.find(user_id);
        if (it != user_idx_by_id_.end()) users_cache_[it->second].last_otp_step = matched_step;
    }
    return ok;
}

bool Database::delete_user(int64_t id) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    auto u = get_user_by_id(id);
    if (!u || u->is_admin() || u->is_guest()) return false;
    Stmt s1(db_, "DELETE FROM x_sessions WHERE user_id = ?1;");
    s1.bind_i64(1, id);
    s1.step();
    Stmt st(db_, "DELETE FROM x_users WHERE id = ?1;");
    st.bind_i64(1, id);
    bool ok = (st.step() == SQLITE_DONE && sqlite3_changes(db_) > 0);
    if (ok) {
        reload_users_cache_locked();
        reload_auth_caches_locked();
    }
    return ok;
}

bool Database::revoke_token_jti(std::string_view jti, int64_t exp_unix) {
    if (jti.empty()) return false;
    std::lock_guard<std::recursive_mutex> lk(mu_);
    int64_t now = unix_now();
    Stmt cleanup(db_, "DELETE FROM x_revoked_tokens WHERE exp_unix <= ?1;");
    cleanup.bind_i64(1, now);
    cleanup.step();

    Stmt st(db_, "INSERT OR REPLACE INTO x_revoked_tokens(jti, exp_unix) VALUES(?1, ?2);");
    st.bind_str(1, jti);
    st.bind_i64(2, exp_unix);
    bool ok = (st.step() == SQLITE_DONE);
    if (ok) {
        std::unique_lock<std::shared_mutex> wlk(cache_mu_);
        revoked_jti_cache_[std::string(jti)] = exp_unix;
    }
    return ok;
}

bool Database::is_token_jti_revoked(std::string_view jti) {
    if (jti.empty()) return false;
    int64_t now = unix_now();
    std::shared_lock<std::shared_mutex> rlk(cache_mu_);
    auto it = revoked_jti_cache_.find(jti);
    return it != revoked_jti_cache_.end() && it->second > now;
}

bool Database::check_or_touch_session(int64_t user_id, std::string_view device_key,
                                      std::string_view ua, std::string_view ip,
                                      bool is_login, std::string* err_out) {
    int64_t now = unix_now();
    std::string map_k = session_map_key(user_id, device_key);

    // Hot Read Fast Path: O(1) shared-lock check without hitting SQLite WAL on every API call!
    if (!is_login) {
        std::shared_lock<std::shared_mutex> rlk(cache_mu_);
        int64_t ttl = 86400;
        if (auto it_ttl = settings_cache_.find("device_session_ttl"); it_ttl != settings_cache_.end()) {
            ttl = parse_i64(it_ttl->second.value).value_or(86400);
        }
        auto it = session_cache_.find(map_k);
        if (it != session_cache_.end()) {
            if (it->second.status == 1) {
                if (err_out) *err_out = "Session has been evicted or logged out";
                return false;
            }
            bool not_expired = (ttl <= 0) || (now - it->second.last_active < ttl);
            if (not_expired && (now - it->second.last_active < 30)) {
                return true; // Coalesce DB touches within 30s window!
            }
        }
    }

    std::lock_guard<std::recursive_mutex> lk(mu_);
    int64_t ttl = get_setting_int("device_session_ttl", 86400);
    if (ttl > 0) {
        Stmt del(db_, "DELETE FROM x_sessions WHERE last_active < ?1;");
        del.bind_i64(1, now - ttl);
        del.step();
    }

    Stmt sel(db_, "SELECT status FROM x_sessions WHERE user_id = ?1 AND device_key = ?2;");
    sel.bind_i64(1, user_id);
    sel.bind_str(2, device_key);
    bool exists = (sel.step() == SQLITE_ROW);
    int cur_status = exists ? sel.col_int(0) : -1;

    if (exists && cur_status == 1 && !is_login) {
        reload_auth_caches_locked();
        if (err_out) *err_out = "Session has been evicted or logged out";
        return false;
    }

    bool needs_slot = (!exists || cur_status == 1);
    if (needs_slot) {
        int64_t max_dev = get_setting_int("max_devices", 0);
        if (max_dev > 0) {
            Stmt cnt(db_, "SELECT COUNT(*) FROM x_sessions WHERE user_id = ?1 AND status = 0;");
            cnt.bind_i64(1, user_id);
            int64_t active_count = (cnt.step() == SQLITE_ROW) ? cnt.col_i64(0) : 0;
            if (active_count >= max_dev) {
                std::string policy = get_setting("device_evict_policy", "evict_oldest");
                if (policy == "evict_oldest") {
                    Stmt ev(db_, "UPDATE x_sessions SET status = 1 WHERE user_id = ?1 AND device_key = ("
                                 "SELECT device_key FROM x_sessions WHERE user_id = ?1 AND status = 0 ORDER BY last_active ASC LIMIT 1);");
                    ev.bind_i64(1, user_id);
                    ev.step();
                } else {
                    if (err_out) *err_out = "Maximum active device sessions reached";
                    return false;
                }
            }
        }
    }

    Stmt up(db_, "INSERT INTO x_sessions(user_id, device_key, user_agent, ip, last_active, status) "
                 "VALUES(?1, ?2, ?3, ?4, ?5, 0) "
                 "ON CONFLICT(user_id, device_key) DO UPDATE SET "
                 "user_agent=excluded.user_agent, ip=excluded.ip, last_active=excluded.last_active, status=0;");
    up.bind_i64(1, user_id);
    up.bind_str(2, device_key);
    up.bind_str(3, ua.substr(0, 256));
    up.bind_str(4, ip.substr(0, 64));
    up.bind_i64(5, now);
    bool ok = (up.step() == SQLITE_DONE);
    reload_auth_caches_locked();
    return ok;
}

std::vector<SessionRecord> Database::list_sessions(std::optional<int64_t> user_id) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    std::vector<SessionRecord> out;
    Stmt st(db_, user_id.has_value()
                     ? "SELECT user_id, device_key, user_agent, ip, last_active, status FROM x_sessions WHERE user_id = ?1 ORDER BY last_active DESC;"
                     : "SELECT user_id, device_key, user_agent, ip, last_active, status FROM x_sessions ORDER BY last_active DESC;");
    if (user_id.has_value()) st.bind_i64(1, *user_id);
    while (st.step() == SQLITE_ROW) {
        SessionRecord s;
        s.user_id     = st.col_i64(0);
        s.device_key  = st.col_str(1);
        s.user_agent  = st.col_str(2);
        s.ip          = st.col_str(3);
        s.last_active = st.col_i64(4);
        s.status      = st.col_int(5);
        out.push_back(std::move(s));
    }
    return out;
}

bool Database::evict_session(int64_t user_id, std::string_view device_key) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    Stmt st(db_, "UPDATE x_sessions SET status = 1 WHERE user_id = ?1 AND device_key = ?2;");
    st.bind_i64(1, user_id);
    st.bind_str(2, device_key);
    bool ok = (st.step() == SQLITE_DONE && sqlite3_changes(db_) > 0);
    if (ok) reload_auth_caches_locked();
    return ok;
}

std::vector<StorageRecord> Database::list_storages(bool enabled_only) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    std::vector<StorageRecord> out;
    Stmt st(db_, enabled_only
                     ? "SELECT id, mount_path, order_idx, driver, cache_expiration, status, addition_enc, remark, "
                       "modified, disabled, enable_sign, order_by, order_direction, extract_folder, web_proxy, "
                       "webdav_policy, down_proxy_url FROM x_storages WHERE disabled = 0 ORDER BY order_idx ASC, id ASC;"
                     : "SELECT id, mount_path, order_idx, driver, cache_expiration, status, addition_enc, remark, "
                       "modified, disabled, enable_sign, order_by, order_direction, extract_folder, web_proxy, "
                       "webdav_policy, down_proxy_url FROM x_storages ORDER BY order_idx ASC, id ASC;");
    while (st.step() == SQLITE_ROW) {
        out.push_back(row_to_storage(st, master_secret_));
    }
    return out;
}

std::optional<StorageRecord> Database::get_storage(int64_t id) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    Stmt st(db_, "SELECT id, mount_path, order_idx, driver, cache_expiration, status, addition_enc, remark, "
                 "modified, disabled, enable_sign, order_by, order_direction, extract_folder, web_proxy, "
                 "webdav_policy, down_proxy_url FROM x_storages WHERE id = ?1;");
    st.bind_i64(1, id);
    if (st.step() != SQLITE_ROW) return std::nullopt;
    return row_to_storage(st, master_secret_);
}

int64_t Database::create_storage(StorageRecord rec) {
    auto mp = clean_vpath(rec.mount_path);
    if (!mp || *mp == "/") return 0;
    std::lock_guard<std::recursive_mutex> lk(mu_);
    Stmt st(db_, "INSERT INTO x_storages(mount_path, order_idx, driver, cache_expiration, status, addition_enc, "
                 "remark, modified, disabled, enable_sign, order_by, order_direction, extract_folder, web_proxy, "
                 "webdav_policy, down_proxy_url) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16);");
    st.bind_str(1, *mp);
    st.bind_int(2, rec.order_idx);
    st.bind_str(3, rec.driver);
    st.bind_int(4, std::max(0, rec.cache_expiration));
    st.bind_str(5, rec.status);
    st.bind_str(6, vault_encrypt(master_secret_, rec.addition.empty() ? "{}" : rec.addition));
    st.bind_str(7, rec.remark);
    st.bind_i64(8, unix_now());
    st.bind_int(9, rec.disabled ? 1 : 0);
    st.bind_int(10, rec.enable_sign ? 1 : 0);
    st.bind_str(11, rec.order_by.empty() ? "name" : rec.order_by);
    st.bind_str(12, rec.order_direction.empty() ? "ASC" : rec.order_direction);
    st.bind_str(13, rec.extract_folder);
    st.bind_int(14, rec.web_proxy ? 1 : 0);
    st.bind_str(15, rec.webdav_policy.empty() ? "native_proxy" : rec.webdav_policy);
    st.bind_str(16, rec.down_proxy_url);
    if (st.step() != SQLITE_DONE) return 0;
    return sqlite3_last_insert_rowid(db_);
}

bool Database::update_storage(const StorageRecord& rec) {
    auto mp = clean_vpath(rec.mount_path);
    if (!mp || *mp == "/") return false;
    std::lock_guard<std::recursive_mutex> lk(mu_);
    Stmt st(db_, "UPDATE x_storages SET mount_path=?1, order_idx=?2, driver=?3, cache_expiration=?4, status=?5, "
                 "addition_enc=?6, remark=?7, modified=?8, disabled=?9, enable_sign=?10, order_by=?11, "
                 "order_direction=?12, extract_folder=?13, web_proxy=?14, webdav_policy=?15, down_proxy_url=?16 "
                 "WHERE id=?17;");
    st.bind_str(1, *mp);
    st.bind_int(2, rec.order_idx);
    st.bind_str(3, rec.driver);
    st.bind_int(4, std::max(0, rec.cache_expiration));
    st.bind_str(5, rec.status);
    st.bind_str(6, vault_encrypt(master_secret_, rec.addition.empty() ? "{}" : rec.addition));
    st.bind_str(7, rec.remark);
    st.bind_i64(8, unix_now());
    st.bind_int(9, rec.disabled ? 1 : 0);
    st.bind_int(10, rec.enable_sign ? 1 : 0);
    st.bind_str(11, rec.order_by);
    st.bind_str(12, rec.order_direction);
    st.bind_str(13, rec.extract_folder);
    st.bind_int(14, rec.web_proxy ? 1 : 0);
    st.bind_str(15, rec.webdav_policy);
    st.bind_str(16, rec.down_proxy_url);
    st.bind_i64(17, rec.id);
    return st.step() == SQLITE_DONE && sqlite3_changes(db_) > 0;
}

bool Database::update_storage_status(int64_t id, std::string_view status) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    Stmt st(db_, "UPDATE x_storages SET status = ?1 WHERE id = ?2;");
    st.bind_str(1, status);
    st.bind_i64(2, id);
    return st.step() == SQLITE_DONE;
}

bool Database::delete_storage(int64_t id) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    Stmt st(db_, "DELETE FROM x_storages WHERE id = ?1;");
    st.bind_i64(1, id);
    return st.step() == SQLITE_DONE && sqlite3_changes(db_) > 0;
}

std::vector<MetaRecord> Database::list_metas() {
    std::shared_lock<std::shared_mutex> rlk(cache_mu_);
    return metas_cache_;
}

std::optional<MetaRecord> Database::get_meta(int64_t id) {
    std::shared_lock<std::shared_mutex> rlk(cache_mu_);
    for (const auto& m : metas_cache_) {
        if (m.id == id) return m;
    }
    return std::nullopt;
}

std::optional<MetaRecord> Database::find_nearest_meta(std::string_view clean_path) {
    std::shared_lock<std::shared_mutex> rlk(cache_mu_);
    for (const auto& m : metas_cache_) {
        if (is_subpath(m.path, clean_path)) {
            return m;
        }
    }
    return std::nullopt;
}

int64_t Database::create_meta(const MetaRecord& meta) {
    auto p = clean_vpath(meta.path);
    if (!p) return 0;
    std::lock_guard<std::recursive_mutex> lk(mu_);
    Stmt st(db_, "INSERT INTO x_metas(path, password, p_sub, write_allow, w_sub, hide, h_sub, readme, r_sub, header, header_sub) "
                 "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11);");
    st.bind_str(1, *p);
    st.bind_str(2, meta.password.empty() ? "" : vault_encrypt(master_secret_, meta.password));
    st.bind_int(3, meta.p_sub ? 1 : 0);
    st.bind_int(4, meta.write ? 1 : 0);
    st.bind_int(5, meta.w_sub ? 1 : 0);
    st.bind_str(6, meta.hide);
    st.bind_int(7, meta.h_sub ? 1 : 0);
    st.bind_str(8, meta.readme);
    st.bind_int(9, meta.r_sub ? 1 : 0);
    st.bind_str(10, meta.header);
    st.bind_int(11, meta.header_sub ? 1 : 0);
    if (st.step() != SQLITE_DONE) return 0;
    int64_t id = sqlite3_last_insert_rowid(db_);
    reload_metas_cache_locked();
    return id;
}

bool Database::update_meta(const MetaRecord& meta) {
    auto p = clean_vpath(meta.path);
    if (!p) return false;
    std::lock_guard<std::recursive_mutex> lk(mu_);
    Stmt st(db_, "UPDATE x_metas SET path=?1, password=?2, p_sub=?3, write_allow=?4, w_sub=?5, hide=?6, h_sub=?7, "
                 "readme=?8, r_sub=?9, header=?10, header_sub=?11 WHERE id=?12;");
    st.bind_str(1, *p);
    st.bind_str(2, meta.password.empty() ? "" : vault_encrypt(master_secret_, meta.password));
    st.bind_int(3, meta.p_sub ? 1 : 0);
    st.bind_int(4, meta.write ? 1 : 0);
    st.bind_int(5, meta.w_sub ? 1 : 0);
    st.bind_str(6, meta.hide);
    st.bind_int(7, meta.h_sub ? 1 : 0);
    st.bind_str(8, meta.readme);
    st.bind_int(9, meta.r_sub ? 1 : 0);
    st.bind_str(10, meta.header);
    st.bind_int(11, meta.header_sub ? 1 : 0);
    st.bind_i64(12, meta.id);
    bool ok = (st.step() == SQLITE_DONE && sqlite3_changes(db_) > 0);
    if (ok) reload_metas_cache_locked();
    return ok;
}

bool Database::delete_meta(int64_t id) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    Stmt st(db_, "DELETE FROM x_metas WHERE id = ?1;");
    st.bind_i64(1, id);
    bool ok = (st.step() == SQLITE_DONE && sqlite3_changes(db_) > 0);
    if (ok) reload_metas_cache_locked();
    return ok;
}

std::vector<ShareRecord> Database::list_shares(std::optional<int64_t> creator_id) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    std::vector<ShareRecord> out;
    Stmt st(db_, creator_id.has_value()
                     ? "SELECT id, share_id, creator_id, creator_name, path, is_dir, password_hash, expires_at, "
                       "max_downloads, downloads, views, disabled, created_at, updated_at FROM x_shares WHERE creator_id=?1 ORDER BY id DESC;"
                     : "SELECT id, share_id, creator_id, creator_name, path, is_dir, password_hash, expires_at, "
                       "max_downloads, downloads, views, disabled, created_at, updated_at FROM x_shares ORDER BY id DESC;");
    if (creator_id.has_value()) st.bind_i64(1, *creator_id);
    while (st.step() == SQLITE_ROW) {
        out.push_back(row_to_share(st));
    }
    return out;
}

std::optional<ShareRecord> Database::get_share(std::string_view share_id) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    Stmt st(db_, "SELECT id, share_id, creator_id, creator_name, path, is_dir, password_hash, expires_at, "
                 "max_downloads, downloads, views, disabled, created_at, updated_at FROM x_shares WHERE share_id = ?1;");
    st.bind_str(1, share_id);
    if (st.step() != SQLITE_ROW) return std::nullopt;
    return row_to_share(st);
}

bool Database::create_share(const ShareRecord& share) {
    auto p = clean_vpath(share.path);
    if (!p || share.share_id.empty()) return false;
    std::lock_guard<std::recursive_mutex> lk(mu_);
    int64_t now = unix_now();
    Stmt st(db_, "INSERT INTO x_shares(share_id, creator_id, creator_name, path, is_dir, password_hash, expires_at, "
                 "max_downloads, downloads, views, disabled, created_at, updated_at) "
                 "VALUES(?1,?2,?3,?4,?5,?6,?7,?8,0,0,?9,?10,?10);");
    st.bind_str(1, share.share_id);
    st.bind_i64(2, share.creator_id);
    st.bind_str(3, share.creator_name);
    st.bind_str(4, *p);
    st.bind_int(5, share.is_dir ? 1 : 0);
    st.bind_str(6, share.password_hash);
    st.bind_i64(7, share.expires_at);
    st.bind_i64(8, share.max_downloads);
    st.bind_int(9, share.disabled ? 1 : 0);
    st.bind_i64(10, now);
    return st.step() == SQLITE_DONE;
}

bool Database::update_share(const ShareRecord& share) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    int64_t now = unix_now();
    Stmt st(db_, "UPDATE x_shares SET password_hash=?1, expires_at=?2, max_downloads=?3, disabled=?4, updated_at=?5 WHERE share_id=?6;");
    st.bind_str(1, share.password_hash);
    st.bind_i64(2, share.expires_at);
    st.bind_i64(3, share.max_downloads);
    st.bind_int(4, share.disabled ? 1 : 0);
    st.bind_i64(5, now);
    st.bind_str(6, share.share_id);
    return st.step() == SQLITE_DONE && sqlite3_changes(db_) > 0;
}

bool Database::increment_share_counter(std::string_view share_id, bool is_download) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    if (is_download) {
        Stmt st(db_, "UPDATE x_shares SET downloads = downloads + 1 WHERE share_id = ?1 "
                     "AND disabled = 0 AND (max_downloads = 0 OR downloads < max_downloads);");
        st.bind_str(1, share_id);
        return st.step() == SQLITE_DONE && sqlite3_changes(db_) > 0;
    } else {
        Stmt st(db_, "UPDATE x_shares SET views = views + 1 WHERE share_id = ?1;");
        st.bind_str(1, share_id);
        return st.step() == SQLITE_DONE;
    }
}

bool Database::delete_share(std::string_view share_id, std::optional<int64_t> restrict_creator) {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    Stmt st(db_, restrict_creator.has_value()
                     ? "DELETE FROM x_shares WHERE share_id = ?1 AND creator_id = ?2;"
                     : "DELETE FROM x_shares WHERE share_id = ?1;");
    st.bind_str(1, share_id);
    if (restrict_creator.has_value()) st.bind_i64(2, *restrict_creator);
    return st.step() == SQLITE_DONE && sqlite3_changes(db_) > 0;
}

void Database::index_directory_entries(std::string_view parent_path, const std::vector<FileObj>& entries) {
    auto p = clean_vpath(parent_path);
    if (!p) return;
    std::lock_guard<std::recursive_mutex> lk(mu_);
    exec_sql("BEGIN IMMEDIATE;");
    {
        Stmt del(db_, "DELETE FROM x_search_fts WHERE parent_path = ?1;");
        del.bind_str(1, *p);
        del.step();

        Stmt ins(db_, "INSERT INTO x_search_fts(name, parent_path, full_path, is_dir, size, file_type) "
                      "VALUES(?1, ?2, ?3, ?4, ?5, ?6);");
        for (const auto& f : entries) {
            auto full = join_vpath(*p, f.name).value_or(*p + "/" + f.name);
            sqlite3_reset(ins.get());
            sqlite3_clear_bindings(ins.get());
            ins.bind_str(1, f.name);
            ins.bind_str(2, *p);
            ins.bind_str(3, full);
            ins.bind_int(4, f.is_dir ? 1 : 0);
            ins.bind_i64(5, f.size);
            ins.bind_int(6, f.type);
            ins.step();
        }
    }
    exec_sql("COMMIT;");
}

void Database::remove_index_prefix(std::string_view path_prefix) {
    auto p = clean_vpath(path_prefix);
    if (!p) return;
    std::lock_guard<std::recursive_mutex> lk(mu_);
    if (*p == "/") {
        clear_search_index();
        return;
    }
    Stmt del1(db_, "DELETE FROM x_search_fts WHERE full_path = ?1 OR (full_path > ?2 AND full_path < ?3);");
    del1.bind_str(1, *p);
    del1.bind_str(2, *p + "/");
    del1.bind_str(3, *p + "0");
    del1.step();
}

void Database::clear_search_index() {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    exec_sql("DELETE FROM x_search_fts;");
}

int64_t Database::count_search_index() {
    std::lock_guard<std::recursive_mutex> lk(mu_);
    Stmt st(db_, "SELECT COUNT(*) FROM x_search_fts;");
    return (st.step() == SQLITE_ROW) ? st.col_i64(0) : 0;
}

std::pair<std::vector<SearchHit>, int64_t> Database::search_files(
    std::string_view keywords,
    std::string_view scope_parent,
    int scope_mode,
    int page,
    int per_page) {

    std::vector<SearchHit> hits;
    keywords = trim_ascii(keywords);
    if (keywords.empty()) return {hits, 0};

    std::string fts_expr;
    for (auto tok : split_sv(keywords, ' ', true)) {
        std::string clean_tok;
        for (char c : tok) {
            if (c != '"' && static_cast<unsigned char>(c) >= 0x20) clean_tok.push_back(c);
        }
        if (clean_tok.empty()) continue;
        if (!fts_expr.empty()) fts_expr += " AND ";
        fts_expr += "name:\"" + clean_tok + "\"*";
    }
    if (fts_expr.empty()) return {hits, 0};

    std::string clean_scope = clean_vpath(scope_parent).value_or("/");
    if (page < 1) page = 1;
    if (per_page < 1 || per_page > 500) per_page = 50;
    int offset = (page - 1) * per_page;

    std::lock_guard<std::recursive_mutex> lk(mu_);
    Stmt st(db_, "SELECT parent_path, name, is_dir, size, file_type FROM x_search_fts "
                 "WHERE x_search_fts MATCH ?1 ORDER BY rank LIMIT 1000;");
    st.bind_str(1, fts_expr);

    std::vector<SearchHit> filtered;
    while (st.step() == SQLITE_ROW) {
        std::string par = st.col_str(0);
        if (!is_subpath(clean_scope, par)) continue;
        bool is_d = st.col_int(2) != 0;
        if (scope_mode == 1 && !is_d) continue;
        if (scope_mode == 2 && is_d) continue;

        SearchHit h;
        h.parent = std::move(par);
        h.name   = st.col_str(1);
        h.is_dir = is_d;
        h.size   = st.col_i64(3);
        h.type   = st.col_int(4);
        filtered.push_back(std::move(h));
    }

    int64_t total = static_cast<int64_t>(filtered.size());
    for (int i = offset; i < static_cast<int>(filtered.size()) && i < offset + per_page; ++i) {
        hits.push_back(std::move(filtered[static_cast<size_t>(i)]));
    }
    return {std::move(hits), total};
}

} // namespace alist
