#include "alist/api_routes.hpp"
#include "alist/cache.hpp"
#include "alist/common.hpp"
#include "alist/config.hpp"
#include "alist/crypto.hpp"
#include "alist/db.hpp"
#include "alist/http_server.hpp"
#include "alist/tasks.hpp"
#include "alist/vfs.hpp"

#include <arpa/inet.h>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace alist;

namespace {

long read_vm_rss_kb() {
    std::ifstream f("/proc/self/status");
    std::string line;
    while (std::getline(f, line)) {
        if (line.starts_with("VmRSS:")) {
            auto sv = trim_ascii(std::string_view(line).substr(6));
            auto sp = sv.find(' ');
            return static_cast<long>(parse_i64(sv.substr(0, sp)).value_or(0));
        }
    }
    return 0;
}

} // namespace

int main() {
    std::signal(SIGPIPE, SIG_IGN);
    std::printf("\n=== AList-Core C++20 Performance & Footprint Benchmark ===\n");
    std::printf("Initial Process VmRSS          : %ld KB (%.2f MB)\n",
                read_vm_rss_kb(), read_vm_rss_kb() / 1024.0);

    // 1. 64-Shard LRU Cache Throughput
    {
        ShardedLruCache<int64_t> cache(65536);
        for (int i = 0; i < 4096; ++i) {
            cache.put("/mount/dir_" + std::to_string(i), i, std::chrono::seconds(300));
        }
        constexpr int kOps = 500000;
        auto t0 = std::chrono::steady_clock::now();
        int64_t sink = 0;
        for (int i = 0; i < kOps; ++i) {
            if (auto v = cache.get("/mount/dir_" + std::to_string(i & 4095))) sink += *v;
        }
        double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("64-Shard LRU Cache Lookups     : %.2f M ops/sec (sink=%lld)\n",
                    (kOps / sec) / 1e6, static_cast<long long>(sink));
    }

    // 2. AES-256-GCM Hardware-Accelerated Throughput
    {
        auto key = derive_key_sha256("bench-secret", "crypt");
        std::array<uint8_t, 12> iv{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
        std::string chunk(65536, 'X');
        constexpr int kChunks = 2048; // 128 MiB
        auto t0 = std::chrono::steady_clock::now();
        size_t bytes = 0;
        for (int i = 0; i < kChunks; ++i) {
            auto ct = aes256_gcm_encrypt_with_iv(key, iv, chunk);
            bytes += ct->size();
        }
        double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("AES-256-GCM Stream Encryption  : %.1f MiB/sec\n",
                    (bytes / (1024.0 * 1024.0)) / sec);
    }

    // 3. HS256 JWT Verification Throughput
    {
        int64_t now = unix_now();
        JwtClaims cl{"admin", 1, 1700000000, now, now, now + 3600, "bench-jti"};
        std::string tok = jwt_encode_hs256(cl, "bench-jwt-secret");
        constexpr int kOps = 100000;
        auto t0 = std::chrono::steady_clock::now();
        int valid = 0;
        for (int i = 0; i < kOps; ++i) {
            if (jwt_decode_hs256(tok, "bench-jwt-secret")) ++valid;
        }
        double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("HS256 JWT Verify Throughput    : %.0f verifications/sec (valid=%d)\n",
                    kOps / sec, valid);
    }

    // 4. SQLite3 WAL + FTS5 Search over 10,000 Indexed Files
    std::string dir = "/tmp/alist_bench_" + random_hex(6);
    std::filesystem::create_directories(dir + "/files");
    ScopeExit cleanup([&] {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    });

    {
        AppConfig cfg;
        cfg.load_or_create(dir + "/data");
        Database db;
        db.init(cfg.db_file, cfg.jwt_secret);

        std::vector<FileObj> entries;
        entries.reserve(10000);
        for (int i = 0; i < 10000; ++i) {
            FileObj f;
            f.name = "project_dataset_shard_" + std::to_string(i) + (i == 7777 ? "_quantum_report.pdf" : ".bin");
            f.is_dir = false;
            f.size = 4096;
            f.type = 4;
            entries.push_back(std::move(f));
        }
        db.index_directory_entries("/local", entries);

        constexpr int kQueries = 2000;
        auto t0 = std::chrono::steady_clock::now();
        int64_t found = 0;
        for (int i = 0; i < kQueries; ++i) {
            auto [hits, total] = db.search_files("quantum", "/", 0, 1, 20);
            found += total;
        }
        double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("SQLite3 FTS5 (10k files)       : %.0f queries/sec (%.1f us/query, hits=%lld)\n",
                    kQueries / sec, (sec * 1e6) / kQueries, static_cast<long long>(found));

        // 5. End-to-End HTTP/1.1 Keep-Alive + Kernel sendfile64() Zero-Copy Benchmark
        std::string big_file = dir + "/files/zero_copy_16mb.bin";
        {
            std::ofstream bf(big_file, std::ios::binary);
            std::string blk(1024 * 1024, 'K');
            for (int i = 0; i < 16; ++i) bf.write(blk.data(), static_cast<std::streamsize>(blk.size()));
        }

        StorageRecord st;
        st.mount_path = "/local";
        st.driver = "Local";
        st.addition = JsonValue::object({{"root_folder_path", dir + "/files"}}).dump();
        db.create_storage(st);

        VfsEngine vfs(db, cfg);
        vfs.reload_all_storages();
        TaskManager tasks(vfs, 2);
        ApiRouter router(cfg, db, vfs, tasks);

        HttpServer server("127.0.0.1", 0, 4, 512);
        server.set_handler([&](HttpRequest& req, HttpResponse& resp) { router.dispatch(req, resp); });
        server.start();
        int port = server.bound_port();

        // Keep-Alive HTTP/1.1 /ping RPS across 4 client threads
        constexpr int kThreads = 4;
        constexpr int kReqsPerThread = 5000;
        auto h0 = std::chrono::steady_clock::now();
        std::atomic<int> pong_verified{0};
        std::vector<std::thread> clients;
        for (int t = 0; t < kThreads; ++t) {
            clients.emplace_back([port, &pong_verified] {
                auto open_conn = [port]() -> int {
                    int s = ::socket(AF_INET, SOCK_STREAM, 0);
                    sockaddr_in sa{};
                    sa.sin_family = AF_INET;
                    sa.sin_port = htons(static_cast<uint16_t>(port));
                    ::inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);
                    if (::connect(s, reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) != 0) {
                        ::close(s);
                        return -1;
                    }
                    return s;
                };
                int fd = open_conn();
                if (fd < 0) return;
                const std::string req = "GET /ping HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: keep-alive\r\n\r\n";
                char rbuf[1024];
                std::string acc;
                for (int i = 0; i < kReqsPerThread; ++i) {
                    if (fd < 0) fd = open_conn();
                    if (fd < 0) break;
                    if (::send(fd, req.data(), req.size(), MSG_NOSIGNAL) <= 0) {
                        ::close(fd);
                        fd = open_conn();
                        if (fd < 0 || ::send(fd, req.data(), req.size(), MSG_NOSIGNAL) <= 0) break;
                    }
                    acc.clear();
                    while (acc.find("\r\n\r\npong") == std::string::npos) {
                        ssize_t n = ::recv(fd, rbuf, sizeof(rbuf), 0);
                        if (n <= 0) {
                            ::close(fd);
                            fd = -1;
                            break;
                        }
                        acc.append(rbuf, static_cast<size_t>(n));
                    }
                    if (acc.ends_with("\r\n\r\npong")) {
                        pong_verified.fetch_add(1, std::memory_order_relaxed);
                        if (acc.find("Connection: close") != std::string::npos) {
                            ::close(fd);
                            fd = -1;
                        }
                    }
                }
                if (fd >= 0) ::close(fd);
            });
        }
        for (auto& th : clients) th.join();
        double hsec = std::chrono::duration<double>(std::chrono::steady_clock::now() - h0).count();
        std::printf("HTTP/1.1 Keep-Alive Throughput : %.0f req/sec (verified=%d/%d)\n",
                    pong_verified.load() / hsec, pong_verified.load(), kThreads * kReqsPerThread);

        // Zero-Copy sendfile64() Throughput (32 x 16 MiB = 512 MiB)
        std::string sys_tok = db.get_setting("token", cfg.jwt_secret);
        std::string sig = sign_url_payload(sys_tok, "/local/zero_copy_16mb.bin", unix_now() + 3600);
        int sfd = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in sa{};
        sa.sin_family = AF_INET;
        sa.sin_port = htons(static_cast<uint16_t>(port));
        ::inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);
        ::connect(sfd, reinterpret_cast<sockaddr*>(&sa), sizeof(sa));

        std::string dl_req = "GET /d/local/zero_copy_16mb.bin?sign=" + url_encode(sig) +
                             " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: keep-alive\r\n\r\n";
        std::vector<char> sink_buf(262144);
        auto z0 = std::chrono::steady_clock::now();
        size_t total_dl = 0;
        for (int i = 0; i < 32; ++i) {
            (void)::send(sfd, dl_req.data(), dl_req.size(), MSG_NOSIGNAL);
            size_t body_Target = 16 * 1024 * 1024;
            size_t recvd = 0;
            bool past_hdr = false;
            std::string hacc;
            while (recvd < body_Target) {
                ssize_t n = ::recv(sfd, sink_buf.data(), sink_buf.size(), 0);
                if (n <= 0) break;
                if (!past_hdr) {
                    hacc.append(sink_buf.data(), static_cast<size_t>(n));
                    auto pos = hacc.find("\r\n\r\n");
                    if (pos != std::string::npos) {
                        past_hdr = true;
                        recvd = hacc.size() - (pos + 4);
                    }
                } else {
                    recvd += static_cast<size_t>(n);
                }
            }
            total_dl += recvd;
        }
        double zsec = std::chrono::duration<double>(std::chrono::steady_clock::now() - z0).count();
        ::close(sfd);

        std::printf("Zero-Copy sendfile64() Stream  : %.1f MiB/sec (%.2f Gbps)\n",
                    (total_dl / (1024.0 * 1024.0)) / zsec,
                    (total_dl * 8.0 / 1e9) / zsec);
        std::printf("Loaded Server Active VmRSS     : %ld KB (%.2f MB)\n",
                    read_vm_rss_kb(), read_vm_rss_kb() / 1024.0);

        server.stop();
        db.close();
    }
    std::printf("============================================================\n\n");
    return 0;
}
