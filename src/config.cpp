#include "alist/config.hpp"
#include "alist/common.hpp"
#include "alist/crypto.hpp"
#include "alist/json.hpp"
#include "alist/models.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

namespace alist {

std::string FileObj::format_iso8601_str() const {
    return format_iso8601(modified > 0 ? modified : unix_now());
}

std::string FileObj::format_iso8601_created() const {
    return format_iso8601(created > 0 ? created : (modified > 0 ? modified : unix_now()));
}

namespace {
std::string env_or(const char* name, const std::string& fallback) {
    const char* v = std::getenv(name);
    return (v && *v) ? std::string(v) : fallback;
}
int bounded_int(const JsonValue& value, int fallback, int minimum, int maximum) {
    const int64_t raw = value.as_i64(fallback);
    return static_cast<int>(std::clamp<int64_t>(raw, minimum, maximum));
}
int env_int_or(const char* name, int fallback, int minimum, int maximum) {
    const char* v = std::getenv(name);
    if (!v || !*v) return fallback;
    auto p = parse_i64(v);
    return p ? static_cast<int>(std::clamp<int64_t>(*p, minimum, maximum)) : fallback;
}
bool env_bool_or(const char* name, bool fallback) {
    const char* v = std::getenv(name);
    if (!v || !*v) return fallback;
    std::string s = to_lower_ascii(v);
    if (s == "1" || s == "true" || s == "yes" || s == "on") return true;
    if (s == "0" || s == "false" || s == "no" || s == "off") return false;
    return fallback;
}
} // namespace

bool AppConfig::load_or_create(const std::string& data_dir_override) {
    if (!data_dir_override.empty()) {
        data_dir = data_dir_override;
    } else {
        data_dir = env_or("ALIST_DATA_DIR", data_dir);
    }

    std::error_code ec;
    std::filesystem::create_directories(data_dir, ec);
    ::chmod(data_dir.c_str(), 0700); // Restrict data dir permissions!

    db_file  = data_dir + "/data.db";
    temp_dir = data_dir + "/temp";
    log_file = data_dir + "/log/alist.log";
    std::string cfg_path = data_dir + "/config.json";

    bool need_save = false;
    if (std::filesystem::exists(cfg_path, ec)) {
        std::ifstream ifs(cfg_path);
        std::ostringstream ss;
        ss << ifs.rdbuf();
        auto j = JsonValue::parse(ss.str());
        if (j && j->is_object()) {
            site_url       = j->get("site_url").as_str(site_url);
            jwt_secret     = j->get("jwt_secret").as_str(jwt_secret);
            token_expires_in = bounded_int(j->get("token_expires_in"), token_expires_in, 1, 87600);
            temp_dir       = j->get("temp_dir").as_str(temp_dir);
            debug          = j->get("debug").as_bool(debug);
            worker_threads = bounded_int(j->get("worker_threads"), worker_threads, 0, 64);
            task_workers   = bounded_int(j->get("task_workers"), task_workers, 1, 16);
            max_connections = bounded_int(j->get("max_connections"), max_connections, 1, 65536);
            allow_private_net_outbound = j->get("allow_private_net_outbound").as_bool(allow_private_net_outbound);
            tls_insecure_skip_verify = j->get("tls_insecure_skip_verify").as_bool(tls_insecure_skip_verify);
            trust_proxy = j->get("trust_proxy").as_bool(trust_proxy);

            const auto& sch = j->get("scheme");
            if (sch.is_object()) {
                address   = sch.get("address").as_str(address);
                http_port = bounded_int(sch.get("http_port"), http_port, 1, 65535);
            }
            const auto& dbj = j->get("database");
            if (dbj.is_object()) {
                db_file = dbj.get("db_file").as_str(db_file);
            }
            const auto& lg = j->get("log");
            if (lg.is_object()) {
                log_enable = lg.get("enable").as_bool(log_enable);
                log_file   = lg.get("name").as_str(log_file);
            }
        }
    } else {
        need_save = true;
    }

    if (jwt_secret.size() < 32) {
        jwt_secret = random_hex(32); // 256-bit CSPRNG secret!
        need_save = true;
    }

    // Environment variable overrides (Docker / Cloud / CI friendly)
    address        = env_or("ALIST_ADDR", env_or("ADDR", address));
    http_port      = env_int_or("ALIST_HTTP_PORT", env_int_or("PORT", http_port, 1, 65535), 1, 65535);
    site_url       = env_or("ALIST_SITE_URL", site_url);
    jwt_secret     = env_or("ALIST_JWT_SECRET", jwt_secret);
    token_expires_in = env_int_or("ALIST_TOKEN_EXPIRES_IN", token_expires_in, 1, 87600);
    debug          = env_bool_or("ALIST_DEBUG", debug);
    allow_private_net_outbound = env_bool_or("ALIST_ALLOW_PRIVATE_NET", allow_private_net_outbound);
    trust_proxy    = env_bool_or("ALIST_TRUST_PROXY", trust_proxy);

    std::filesystem::create_directories(temp_dir, ec);
    ::chmod(temp_dir.c_str(), 0700);
    if (log_enable && !log_file.empty()) {
        std::filesystem::create_directories(std::filesystem::path(log_file).parent_path(), ec);
    }

    if (need_save) {
        save(cfg_path);
    }
    return true;
}

bool AppConfig::save(const std::string& config_path) const {
    JsonValue root = JsonValue::object({
        {"site_url",         site_url},
        {"jwt_secret",       jwt_secret},
        {"token_expires_in", token_expires_in},
        {"temp_dir",         temp_dir},
        {"debug",            debug},
        {"worker_threads",   worker_threads},
        {"task_workers",     task_workers},
        {"max_connections",  max_connections},
        {"allow_private_net_outbound", allow_private_net_outbound},
        {"tls_insecure_skip_verify",   tls_insecure_skip_verify},
        {"trust_proxy",                trust_proxy},
        {"scheme", JsonValue::object({
            {"address",   address},
            {"http_port", http_port},
        })},
        {"database", JsonValue::object({
            {"type",    "sqlite3"},
            {"db_file", db_file},
        })},
        {"log", JsonValue::object({
            {"enable", log_enable},
            {"name",   log_file},
        })},
    });

    std::string tmp_path = config_path + ".tmp";
    {
        std::ofstream ofs(tmp_path, std::ios::out | std::ios::trunc);
        if (!ofs.is_open()) return false;
        ofs << root.dump(true) << "\n";
    }
    ::chmod(tmp_path.c_str(), 0600); // Protect jwt_secret in config.json!
    std::error_code ec;
    std::filesystem::rename(tmp_path, config_path, ec);
    return !ec;
}

} // namespace alist
