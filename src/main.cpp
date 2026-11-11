#include "alist/api_routes.hpp"
#include "alist/common.hpp"
#include "alist/config.hpp"
#include "alist/crypto.hpp"
#include "alist/db.hpp"
#include "alist/http_server.hpp"
#include "alist/logger.hpp"
#include "alist/tasks.hpp"
#include "alist/version.hpp"
#include "alist/vfs.hpp"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <thread>

namespace {
std::atomic<bool> g_quit{false};
void on_signal(int) { g_quit.store(true); }

void print_usage() {
    std::printf(
        "%s %s (%s)\n\n"
        "Usage:\n"
        "  alist server [--data <dir>] [--port <port>] [--debug]\n"
        "  alist admin random [--data <dir>]\n"
        "  alist admin set <new_password> [--data <dir>]\n"
        "  alist storage list [--data <dir>]\n"
        "  alist version\n",
        alist::kAppName, alist::kVersion, alist::kEngine);
}
} // namespace

int main(int argc, char** argv) {
    std::signal(SIGPIPE, SIG_IGN);

    std::string subcmd = (argc >= 2) ? argv[1] : "server";
    if (subcmd == "-h" || subcmd == "--help" || subcmd == "help") {
        print_usage();
        return 0;
    }
    if (subcmd == "version" || subcmd == "-v" || subcmd == "--version") {
        std::printf("%s %s\nEngine: %s\nWeb UI: %s\n",
                    alist::kAppName, alist::kVersion, alist::kEngine, alist::kWebVersion);
        return 0;
    }

    std::string data_dir;
    int port_override = -1;
    bool debug_flag = false;
    std::vector<std::string> pos_args;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--data" && i + 1 < argc) {
            data_dir = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port_override = static_cast<int>(alist::parse_i64(argv[++i]).value_or(5244));
        } else if (arg == "--debug") {
            debug_flag = true;
        } else {
            pos_args.push_back(std::move(arg));
        }
    }
    if (!pos_args.empty()) subcmd = pos_args[0];

    alist::AppConfig cfg;
    cfg.load_or_create(data_dir);
    if (port_override > 0) cfg.http_port = port_override;
    if (debug_flag) cfg.debug = true;

    alist::Logger::instance().set_level(cfg.debug ? alist::LogLevel::Debug : alist::LogLevel::Info);
    if (cfg.log_enable && !cfg.log_file.empty()) {
        alist::Logger::instance().set_file(cfg.log_file);
    }

    alist::Database db;
    std::string init_admin_pwd;
    if (!db.init(cfg.db_file, cfg.jwt_secret, &init_admin_pwd)) {
        std::fprintf(stderr, "Fatal: Failed initializing database at %s\n", cfg.db_file.c_str());
        return 1;
    }

    if (!init_admin_pwd.empty()) {
        LOG_INFO("bootstrap", "Initial administrator account created -> username: admin | password: " + init_admin_pwd);
    }

    if (subcmd == "admin") {
        std::string action = (pos_args.size() >= 2) ? pos_args[1] : "random";
        auto admin = db.get_admin_user();
        if (!admin) {
            std::fprintf(stderr, "Admin user not found\n");
            return 1;
        }
        std::string new_pw;
        if (action == "set" && pos_args.size() >= 3) {
            new_pw = pos_args[2];
        } else if (action == "random") {
            new_pw = alist::random_base64url(12);
        } else {
            print_usage();
            return 1;
        }
        if (new_pw.size() < 6) {
            std::fprintf(stderr, "Error: Password must be at least 6 characters.\n");
            return 1;
        }
        db.update_user_password(admin->id, new_pw);
        std::printf("Successfully updated admin password:\n  username: admin\n  password: %s\n", new_pw.c_str());
        return 0;
    }

    if (subcmd == "storage") {
        auto list = db.list_storages(false);
        std::printf("%-5s %-24s %-14s %-10s\n", "ID", "MOUNT_PATH", "DRIVER", "STATUS");
        for (const auto& s : list) {
            std::printf("%-5lld %-24s %-14s %-10s\n",
                        static_cast<long long>(s.id), s.mount_path.c_str(), s.driver.c_str(), s.status.c_str());
        }
        return 0;
    }

    if (subcmd != "server") {
        print_usage();
        return 1;
    }

    alist::VfsEngine vfs(db, cfg);
    vfs.reload_all_storages();

    alist::TaskManager tasks(vfs, cfg.task_workers);
    alist::ApiRouter router(cfg, db, vfs, tasks);

    alist::HttpServer server(cfg.address, cfg.http_port, cfg.worker_threads, cfg.max_connections);
    server.set_download_limiter(&router.download_limiter());
    server.set_stream_predicate([&router](const alist::HttpRequest& r) {
        return router.should_stream_request_body(r);
    });
    server.set_handler([&router](alist::HttpRequest& req, alist::HttpResponse& resp) {
        router.dispatch(req, resp);
    });

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    if (!server.start()) {
        LOG_ERROR("server", "Failed starting HTTP server");
        return 1;
    }

    LOG_INFO("server", std::string(alist::kAppName) + " " + alist::kVersion +
             " listening on http://" + cfg.address + ":" + std::to_string(server.bound_port()));

    while (!g_quit.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }

    LOG_INFO("server", "Shutting down gracefully...");
    server.stop();
    db.close();
    LOG_INFO("server", "Shutdown complete.");
    return 0;
}
