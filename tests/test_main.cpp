#include "alist/api_routes.hpp"
#include "alist/archive.hpp"
#include "alist/cache.hpp"
#include "alist/common.hpp"
#include "alist/config.hpp"
#include "alist/crypto.hpp"
#include "alist/db.hpp"
#include "alist/http_client.hpp"
#include "alist/http_server.hpp"
#include "alist/json.hpp"
#include "alist/security.hpp"
#include "alist/tasks.hpp"
#include "alist/vfs.hpp"
#include "alist/xml.hpp"
#include "c_libs.hpp"

#include <arpa/inet.h>
#include <atomic>
#include <cassert>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace alist;

namespace {

int g_tests_run = 0;
std::atomic<int> g_assertions{0};

#define CHECK(cond)                                                                                  \
    do {                                                                                             \
        g_assertions.fetch_add(1, std::memory_order_relaxed);                                        \
        if (!(cond)) {                                                                               \
            std::fprintf(stderr, "\n[FAIL] %s:%d: %s\n", __FILE__, __LINE__, #cond);                 \
            std::abort();                                                                            \
        }                                                                                            \
    } while (0)

#define RUN_TEST(fn)                                                                                 \
    do {                                                                                             \
        ++g_tests_run;                                                                               \
        std::printf("  [RUN ] %-52s ... ", #fn);                                                     \
        std::fflush(stdout);                                                                         \
        fn();                                                                                        \
        std::printf("OK\n");                                                                         \
    } while (0)

std::string build_test_zip(std::string_view entry_name, std::string_view payload, bool corrupt_crc = false) {
    uint32_t crc = static_cast<uint32_t>(::crc32(0L, nullptr, 0));
    crc = static_cast<uint32_t>(::crc32(crc, reinterpret_cast<const Bytef*>(payload.data()),
                                        static_cast<uInt>(payload.size())));
    if (corrupt_crc) crc ^= 0xDEADBEEFu;

    z_stream zs{};
    CHECK(alist_deflateInit2(&zs, 6, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) == Z_OK);
    zs.next_in = reinterpret_cast<const Bytef*>(payload.data());
    zs.avail_in = static_cast<uInt>(payload.size());
    std::string comp(payload.size() + 64, '\0');
    zs.next_out = reinterpret_cast<Bytef*>(comp.data());
    zs.avail_out = static_cast<uInt>(comp.size());
    int zrc = deflate(&zs, Z_FINISH);
    CHECK(zrc == Z_STREAM_END);
    comp.resize(zs.total_out);
    deflateEnd(&zs);

    auto put16 = [](std::string& s, uint16_t v) {
        s.push_back(static_cast<char>(v & 0xFF));
        s.push_back(static_cast<char>((v >> 8) & 0xFF));
    };
    auto put32 = [](std::string& s, uint32_t v) {
        s.push_back(static_cast<char>(v & 0xFF));
        s.push_back(static_cast<char>((v >> 8) & 0xFF));
        s.push_back(static_cast<char>((v >> 16) & 0xFF));
        s.push_back(static_cast<char>((v >> 24) & 0xFF));
    };

    std::string z;
    put32(z, 0x04034b50u);
    put16(z, 20);
    put16(z, 0);
    put16(z, 8);
    put16(z, 0);
    put16(z, 0);
    put32(z, crc);
    put32(z, static_cast<uint32_t>(comp.size()));
    put32(z, static_cast<uint32_t>(payload.size()));
    put16(z, static_cast<uint16_t>(entry_name.size()));
    put16(z, 0);
    z.append(entry_name);
    z.append(comp);

    uint32_t cd_offset = static_cast<uint32_t>(z.size());
    put32(z, 0x02014b50u);
    put16(z, 20);
    put16(z, 20);
    put16(z, 0);
    put16(z, 8);
    put16(z, 0);
    put16(z, 0);
    put32(z, crc);
    put32(z, static_cast<uint32_t>(comp.size()));
    put32(z, static_cast<uint32_t>(payload.size()));
    put16(z, static_cast<uint16_t>(entry_name.size()));
    put16(z, 0);
    put16(z, 0);
    put16(z, 0);
    put16(z, 0);
    put32(z, 0);
    put32(z, 0);
    z.append(entry_name);

    uint32_t cd_size = static_cast<uint32_t>(z.size()) - cd_offset;
    put32(z, 0x06054b50u);
    put16(z, 0);
    put16(z, 0);
    put16(z, 1);
    put16(z, 1);
    put32(z, cd_size);
    put32(z, cd_offset);
    put16(z, 0);
    return z;
}

struct RawHttpReply {
    int status = 0;
    std::map<std::string, std::string> headers;
    std::string body;
};

RawHttpReply raw_http_call(int port, std::string_view raw_request) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    CHECK(fd >= 0);
    sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(static_cast<uint16_t>(port));
    ::inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);
    CHECK(::connect(fd, reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) == 0);

    size_t sent = 0;
    while (sent < raw_request.size()) {
        ssize_t n = ::send(fd, raw_request.data() + sent, raw_request.size() - sent, MSG_NOSIGNAL);
        CHECK(n > 0);
        sent += static_cast<size_t>(n);
    }

    std::string resp;
    char buf[8192];
    while (true) {
        ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
        if (n <= 0) break;
        resp.append(buf, static_cast<size_t>(n));
    }
    ::close(fd);

    RawHttpReply out;
    auto hdr_end = resp.find("\r\n\r\n");
    if (hdr_end == std::string::npos) return out;
    std::string_view head(resp.data(), hdr_end);
    out.body = resp.substr(hdr_end + 4);

    auto lines = split_sv(head, '\n');
    if (!lines.empty()) {
        auto sp1 = lines[0].find(' ');
        if (sp1 != std::string_view::npos) {
            auto rest = lines[0].substr(sp1 + 1);
            auto sp2 = rest.find(' ');
            out.status = static_cast<int>(parse_i64(rest.substr(0, sp2)).value_or(0));
        }
    }
    for (size_t i = 1; i < lines.size(); ++i) {
        auto ln = trim_ascii(lines[i]);
        auto col = ln.find(':');
        if (col != std::string_view::npos) {
            out.headers[to_lower_ascii(trim_ascii(ln.substr(0, col)))] =
                std::string(trim_ascii(ln.substr(col + 1)));
        }
    }
    return out;
}

// ============================================================================
// 1. Path Normalization, Traversal & Symlink Confinement Tests
// ============================================================================
void test_vpath_and_symlink_security() {
    CHECK(clean_vpath("/a/b/../c//d/.").value() == "/a/c/d");
    CHECK(clean_vpath("/../../../../etc/passwd").value() == "/etc/passwd");
    CHECK(!clean_vpath(std::string_view("/bad\0path", 9)).has_value());
    CHECK(!clean_vpath("/\x01\x02").has_value());
    CHECK(is_subpath("/home/alice", "/home/alice/docs/a.txt"));
    CHECK(!is_subpath("/home/alice", "/home/alice_evil/a.txt")); // Boundary prefix collision!

    CHECK(!is_valid_filename(".."));
    CHECK(!is_valid_filename("."));
    CHECK(!is_valid_filename("a/b"));
    CHECK(!is_valid_filename("a\\b"));
    CHECK(is_valid_filename("report-2026_v1.pdf"));

    std::string root = "/tmp/alist_sec_test_" + random_hex(6);
    std::string outside = "/tmp/alist_outside_" + random_hex(6);
    std::filesystem::create_directories(root + "/sub");
    std::filesystem::create_directories(outside);
    ScopeExit cleanup([&] {
        std::error_code ec;
        std::filesystem::remove_all(root, ec);
        std::filesystem::remove_all(outside, ec);
    });

    { std::ofstream f(outside + "/secret.txt"); f << "TOP_SECRET_OUTSIDE"; }
    { std::ofstream f(root + "/sub/safe.txt");  f << "SAFE_CONTENT"; }

    CHECK(::symlink((outside + "/secret.txt").c_str(), (root + "/evil_file_link").c_str()) == 0);
    CHECK(::symlink(outside.c_str(), (root + "/evil_dir_link").c_str()) == 0);

    char* rp = ::realpath(root.c_str(), nullptr);
    CHECK(rp != nullptr);
    std::string canon_root(rp);
    std::free(rp);

    CHECK(resolve_confined_local_path(canon_root, "/sub/safe.txt", false).has_value());
    CHECK(!resolve_confined_local_path(canon_root, "/evil_file_link", false).has_value());
    CHECK(!resolve_confined_local_path(canon_root, "/evil_dir_link/secret.txt", false).has_value());
    CHECK(!resolve_confined_local_path(canon_root, "/evil_dir_link/new_write.txt", false).has_value());
    // Even if allow_symlinks=true, confined path check MUST still block outside-root targets!
    CHECK(!resolve_confined_local_path(canon_root, "/evil_file_link", true).has_value());
}

// ============================================================================
// 2. Cryptography, JWT Algorithm Pinning, Vault & TOTP Replay Protection
// ============================================================================
void test_crypto_jwt_and_totp_security() {
    auto key = derive_key_sha256("master-secret-test", "domain-v1");
    std::string plain = "Sensitive cloud refresh_token=1//0gABCDEF_secret";
    std::string aad   = "x_storages";

    auto ct_opt = aes256_gcm_encrypt(key, plain, aad);
    CHECK(ct_opt.has_value() && !ct_opt->empty());
    auto dec = aes256_gcm_decrypt(key, *ct_opt, aad);
    CHECK(dec.has_value() && *dec == plain);

    // Flip 1 bit in ciphertext -> MUST fail authentication!
    std::string tampered = *ct_opt;
    tampered[tampered.size() / 2] ^= 0x01;
    CHECK(!aes256_gcm_decrypt(key, tampered, aad).has_value());
    // Wrong AAD -> MUST fail authentication!
    CHECK(!aes256_gcm_decrypt(key, *ct_opt, "wrong_aad").has_value());

    // Password PBKDF2-HMAC-SHA256
    std::string ph = hash_password_pbkdf2("CorrectHorseBatteryStaple!");
    CHECK(verify_password_pbkdf2("CorrectHorseBatteryStaple!", ph));
    CHECK(!verify_password_pbkdf2("WrongPassword!", ph));

    // JWT HS256 & Algorithm Confusion Attack ("alg":"none")
    int64_t now = unix_now();
    JwtClaims c{"admin", 1, 1700000000, now, now, now + 3600, "jti-123"};
    std::string jwt = jwt_encode_hs256(c, "super-jwt-key");
    auto parsed = jwt_decode_hs256(jwt, "super-jwt-key");
    CHECK(parsed.has_value() && parsed->username == "admin" && parsed->jti == "jti-123");
    CHECK(!jwt_decode_hs256(jwt, "wrong-jwt-key").has_value());

    // Forged "alg":"none" token MUST be rejected!
    std::string none_hdr = base64url_encode(R"({"alg":"none","typ":"JWT"})");
    std::string payload(split_sv(jwt, '.')[1]);
    std::string forged_none = none_hdr + "." + payload + ".";
    CHECK(!jwt_decode_hs256(forged_none, "super-jwt-key").has_value());
    CHECK(!jwt_decode_hs256(forged_none, "").has_value());

    // Expired JWT MUST be rejected!
    JwtClaims exp_c{"admin", 1, 1700000000, now - 7200, now - 7200, now - 10, "jti-exp"};
    CHECK(!jwt_decode_hs256(jwt_encode_hs256(exp_c, "super-jwt-key"), "super-jwt-key").has_value());

    // TOTP RFC 6238 generation & verification
    std::string otp_sec = totp_generate_secret();
    CHECK(otp_sec.size() >= 24);
    std::string code = totp_code_at(otp_sec, now);
    CHECK(code.size() == 6);
    int64_t step = -1;
    CHECK(totp_verify(otp_sec, code, now, &step));
    CHECK(step == now / 30);

    // Gzip roundtrip
    std::string sample(4096, 'Z');
    auto gz = gzip_compress(sample, 6);
    CHECK(gz.has_value() && gz->size() < 200);
}

// ============================================================================
// 3. SSRF Protection & Linear-Time Anti-ReDoS Glob Matcher
// ============================================================================
void test_ssrf_and_antiredos_glob() {
    CHECK(!validate_outbound_url("http://127.0.0.1:8080/admin", false));
    CHECK(!validate_outbound_url("http://localhost/secret", false));
    CHECK(!validate_outbound_url("http://169.254.169.254/latest/meta-data/", false));
    CHECK(!validate_outbound_url("http://10.0.0.5/internal", false));
    CHECK(!validate_outbound_url("http://192.168.1.1/router", false));
    CHECK(!validate_outbound_url("http://172.20.0.1/cluster", false));
    CHECK(!validate_outbound_url("http://[::1]:5244/ping", false));
    CHECK(!validate_outbound_url("file:///etc/passwd", false));
    CHECK(!validate_outbound_url("gopher://127.0.0.1:6379/_INFO", false));
    CHECK(validate_outbound_url("http://127.0.0.1:8080/ok", true)); // Explicit opt-in only

    // Pathological ReDoS input that hangs backtracking regex engines for hours
    std::string evil_text(250, 'a');
    std::string evil_pattern = "*a*a*a*a*a*a*a*a*a*a*a*a*a*a*b";
    auto t0 = std::chrono::steady_clock::now();
    bool matched = linear_glob_match(evil_pattern, evil_text);
    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    CHECK(!matched);
    CHECK(elapsed_us < 20000);

    CHECK(matches_any_hide_pattern("*.bak\nsecret_*\n.git", "secret_keys.pem"));
    CHECK(!matches_any_hide_pattern("*.bak\nsecret_*\n.git", "public_readme.md"));
}

// ============================================================================
// 4. Bounded JSON & XML Parser Security Tests
// ============================================================================
void test_json_and_xml_bounds() {
    std::string deep_json(100, '[');
    deep_json += "1";
    deep_json += std::string(100, ']');
    CHECK(!JsonValue::parse(deep_json).has_value());

    auto j = JsonValue::parse(R"({"emoji":"\uD83D\uDE80","num":42,"arr":[true,false,null]})");
    CHECK(j.has_value());
    CHECK(j->get("num").as_i64() == 42);
    CHECK(j->get("emoji").as_str().size() == 4);

    std::string xxe_xml = R"(<?xml version="1.0"?><!DOCTYPE foo [<!ENTITY xxe SYSTEM "file:///etc/passwd">]><D:propfind xmlns:D="DAV:"><D:allprop/></D:propfind>)";
    CHECK(!parse_xml(xxe_xml).has_value()); // Strictly rejects DTD/ENTITY (OWASP XXE defense)!

    std::string valid_xml = R"(<?xml version="1.0"?><D:propfind xmlns:D="DAV:"><D:allprop/></D:propfind>)";
    auto xr = parse_xml(valid_xml);
    CHECK(xr.has_value() && xr->local_name() == "propfind");
}

// ============================================================================
// 5. Database At-Rest Encryption, TOTP Replay DB Guard & FTS5 Search
// ============================================================================
void test_database_vault_and_fts5() {
    std::string dir = "/tmp/alist_db_test_" + random_hex(6);
    std::filesystem::create_directories(dir);
    ScopeExit cleanup([&] {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    });

    Database db;
    std::string init_pw;
    std::string db_path = dir + "/data.db";
    CHECK(db.init(db_path, "vault-master-secret-key-2026", &init_pw));
    CHECK(!init_pw.empty());

    StorageRecord st;
    st.mount_path = "/s3test";
    st.driver = "S3";
    st.addition = R"({"bucket":"my-bkt","secret_access_key":"ULTRA_SENSITIVE_AWS_KEY_999"})";
    int64_t sid = db.create_storage(st);
    CHECK(sid > 0);

    // Verify raw SQLite bytes on disk DO NOT contain plaintext secret!
    {
        std::ifstream raw_in(db_path, std::ios::binary);
        std::string raw_bytes((std::istreambuf_iterator<char>(raw_in)), std::istreambuf_iterator<char>());
        std::ifstream wal_in(db_path + "-wal", std::ios::binary);
        std::string wal_bytes((std::istreambuf_iterator<char>(wal_in)), std::istreambuf_iterator<char>());
        CHECK(raw_bytes.find("ULTRA_SENSITIVE_AWS_KEY_999") == std::string::npos);
        CHECK(wal_bytes.find("ULTRA_SENSITIVE_AWS_KEY_999") == std::string::npos);
    }
    auto loaded = db.get_storage(sid);
    CHECK(loaded.has_value());
    CHECK(loaded->addition.find("ULTRA_SENSITIVE_AWS_KEY_999") != std::string::npos);

    // Atomic TOTP step replay guard
    auto admin = db.get_admin_user();
    CHECK(admin.has_value());
    CHECK(db.verify_and_advance_otp_step(admin->id, 55000000));
    CHECK(!db.verify_and_advance_otp_step(admin->id, 55000000)); // Replay blocked!
    CHECK(!db.verify_and_advance_otp_step(admin->id, 54999999)); // Older step blocked!
    CHECK(db.verify_and_advance_otp_step(admin->id, 55000001));  // Next window succeeds!

    // FTS5 Full-Text Search
    FileObj f1; f1.name = "architecture_spec_2026.pdf"; f1.is_dir = false; f1.size = 1048576; f1.type = 4;
    FileObj f2; f2.name = "demo_presentation.mp4";      f2.is_dir = false; f2.size = 52428800; f2.type = 2;
    db.index_directory_entries("/docs", {f1, f2});
    auto [hits, total] = db.search_files("architecture", "/docs", 0, 1, 20);
    CHECK(total == 1 && hits.size() == 1 && hits[0].name == "architecture_spec_2026.pdf");
    db.close();
}

// ============================================================================
// 6. VFS Engine, Crypt Streaming AES-256-GCM Seek & Alias Loop Guard
// ============================================================================
void test_vfs_crypt_driver_and_loop_guard() {
    std::string dir = "/tmp/alist_vfs_test_" + random_hex(6);
    std::filesystem::create_directories(dir + "/backing/enc_store");
    ScopeExit cleanup([&] {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    });

    AppConfig cfg;
    cfg.load_or_create(dir + "/data");
    Database db;
    CHECK(db.init(cfg.db_file, cfg.jwt_secret));
    VfsEngine vfs(db, cfg);

    StorageRecord s_local;
    s_local.mount_path = "/local";
    s_local.driver = "Local";
    s_local.addition = JsonValue::object({{"root_folder_path", dir + "/backing"}}).dump();
    CHECK(vfs.reload_storage(db.create_storage(s_local)));

    StorageRecord s_crypt;
    s_crypt.mount_path = "/vault";
    s_crypt.driver = "Crypt";
    s_crypt.addition = JsonValue::object({
        {"target_path", "/local/enc_store"},
        {"password", "ZeroKnowledgeVaultKey#2026"},
        {"salt", "custom-salt-v1"},
        {"encrypt_filenames", true}
    }).dump();
    CHECK(vfs.reload_storage(db.create_storage(s_crypt)));

    // 1) Direct self-referential Alias /loop -> /loop MUST be rejected right at init()!
    StorageRecord s_loop;
    s_loop.mount_path = "/loop";
    s_loop.driver = "Alias";
    s_loop.addition = R"({"target_path":"/loop"})";
    CHECK(!vfs.reload_storage(db.create_storage(s_loop)));

    // 2) Mutual 2-hop Alias cycle (/loopA -> /loopB -> /loopA) MUST abort cleanly without deadlock or stack overflow!
    StorageRecord s_loop_a;
    s_loop_a.mount_path = "/loopA";
    s_loop_a.driver = "Alias";
    s_loop_a.addition = R"({"target_path":"/loopB"})";
    StorageRecord s_loop_b;
    s_loop_b.mount_path = "/loopB";
    s_loop_b.driver = "Alias";
    s_loop_b.addition = R"({"target_path":"/loopA"})";
    CHECK(vfs.reload_storage(db.create_storage(s_loop_a)));
    CHECK(vfs.reload_storage(db.create_storage(s_loop_b)));
    auto loop_res = vfs.list_dir(nullptr, "/loopA", "", true);
    CHECK(!loop_res.ok);

    // Stream upload a 200,000-byte payload (> 3 x 64 KiB chunks) into /vault/movie.mp4
    std::string original(200000, '\0');
    for (size_t i = 0; i < original.size(); ++i) {
        original[i] = static_cast<char>((i * 131 + 17) & 0xFF);
    }

    size_t read_pos = 0;
    UploadStream up;
    up.filename = "movie.mp4";
    up.size = static_cast<int64_t>(original.size());
    up.read_fn = [&](char* b, size_t m) -> ssize_t {
        if (read_pos >= original.size()) return 0;
        size_t n = std::min(m, original.size() - read_pos);
        std::memcpy(b, original.data() + read_pos, n);
        read_pos += n;
        return static_cast<ssize_t>(n);
    };
    std::string err;
    CHECK(vfs.put_stream("/vault/movie.mp4", up, err));

    // Verify filename on disk is encrypted!
    bool found_cipher_file = false;
    std::string cipher_disk_path;
    for (const auto& ent : std::filesystem::directory_iterator(dir + "/backing/enc_store")) {
        std::string fn = ent.path().filename().string();
        CHECK(fn != "movie.mp4" && fn.starts_with("c1_"));
        found_cipher_file = true;
        cipher_disk_path = ent.path().string();
    }
    CHECK(found_cipher_file);

    // Verify VFS listing & exact decrypted size calculation
    auto ls = vfs.list_dir(nullptr, "/vault", "", true);
    CHECK(ls.ok && ls.entries.size() == 1);
    CHECK(ls.entries[0].name == "movie.mp4" && ls.entries[0].size == 200000);

    // Verify O(1) cross-chunk Range seek [65500..131150] (crossing chunks 0, 1, and 2!)
    std::map<std::string, std::string> req_hdrs;
    auto dl = vfs.resolve_download("/vault/movie.mp4", req_hdrs, err);
    CHECK(dl.has_value() && dl->mode == DownloadResolution::Mode::CustomStream);
    std::string range_out;
    int64_t want_len = 131150 - 65500 + 1;
    bool stream_ok = dl->stream_fn(65500, want_len, [&](const char* p, size_t n) {
        range_out.append(p, n);
        return true;
    });
    CHECK(stream_ok);
    CHECK(static_cast<int64_t>(range_out.size()) == want_len);
    CHECK(range_out == original.substr(65500, static_cast<size_t>(want_len)));

    // Tamper 1 byte in encrypted backing file -> GCM stream MUST detect corruption (false)!
    {
        int fd = ::open(cipher_disk_path.c_str(), O_RDWR);
        CHECK(fd >= 0);
        uint8_t b = 0;
        CHECK(::pread(fd, &b, 1, 100) == 1);
        b ^= 0xFF;
        CHECK(::pwrite(fd, &b, 1, 100) == 1);
        ::close(fd);
    }
    auto bad_dl = vfs.resolve_download("/vault/movie.mp4", req_hdrs, err);
    CHECK(bad_dl.has_value());
    CHECK(!bad_dl->stream_fn(0, 200000, [&](const char*, size_t) { return true; })); // Tamper caught!
    db.close();
}

// ============================================================================
// 7. ZIP Archive Extraction, Zip-Slip & Corrupted CRC-32 Defense
// ============================================================================
void test_zip_archive_security() {
    std::string dir = "/tmp/alist_zip_test_" + random_hex(6);
    std::filesystem::create_directories(dir + "/out");
    ScopeExit cleanup([&] {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    });

    std::string good_zip = build_test_zip("docs/hello.txt", "Hello from AList-Core ZIP Engine!");
    std::string good_path = dir + "/good.zip";
    { std::ofstream f(good_path, std::ios::binary); f << good_zip; }

    std::vector<ZipEntryInfo> entries;
    std::string err;
    CHECK(zip_list_entries(good_path, entries, err));
    CHECK(entries.size() == 1 && entries[0].path == "docs/hello.txt");
    auto ext = zip_extract_entry(good_path, "docs/hello.txt", err);
    CHECK(ext.has_value() && *ext == "Hello from AList-Core ZIP Engine!");
    CHECK(zip_decompress_to_dir(good_path, dir + "/out", err));
    CHECK(std::filesystem::exists(dir + "/out/docs/hello.txt"));

    // Malicious Zip-Slip archive ("../../../evil_slip.sh") -> MUST NOT escape!
    std::string slip_zip = build_test_zip("../../../evil_slip.sh", "pwned");
    std::string slip_path = dir + "/slip.zip";
    { std::ofstream f(slip_path, std::ios::binary); f << slip_zip; }
    CHECK(!zip_decompress_to_dir(slip_path, dir + "/out", err));
    CHECK(!std::filesystem::exists(dir + "/evil_slip.sh"));

    // Corrupted CRC-32 ZIP -> MUST fail integrity check!
    std::string bad_crc_zip = build_test_zip("a.txt", "Integrity Check Payload", true);
    std::string bad_crc_path = dir + "/badcrc.zip";
    { std::ofstream f(bad_crc_path, std::ios::binary); f << bad_crc_zip; }
    CHECK(!zip_extract_entry(bad_crc_path, "a.txt", err).has_value());
}

// ============================================================================
// 8. End-to-End Live HTTP Server + API + RBAC + WebDAV + Concurrency Hammer
// ============================================================================
void test_live_http_api_webdav_and_concurrency() {
    std::string dir = "/tmp/alist_e2e_test_" + random_hex(6);
    std::filesystem::create_directories(dir + "/storage/alice_home");
    std::filesystem::create_directories(dir + "/storage/secret_vault");
    ScopeExit cleanup([&] {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    });

    { std::ofstream f(dir + "/storage/secret_vault/confidential.txt"); f << "TOP_SECRET_DOCUMENT_0123456789"; }
    { std::ofstream f(dir + "/storage/alice_home/xss.html");           f << "<script>alert('xss')</script>"; }

    AppConfig cfg;
    cfg.load_or_create(dir + "/data");
    cfg.address = "127.0.0.1";
    cfg.http_port = 0;
    cfg.worker_threads = 4;

    Database db;
    std::string admin_pw;
    CHECK(db.init(cfg.db_file, cfg.jwt_secret, &admin_pw));
    db.update_user_password(1, "AdminPass#2026");

    StorageRecord st;
    st.mount_path = "/local";
    st.driver = "Local";
    st.addition = JsonValue::object({{"root_folder_path", dir + "/storage"}}).dump();
    db.create_storage(st);

    MetaRecord meta;
    meta.path = "/local/secret_vault";
    meta.password = "FolderPin999";
    meta.p_sub = true;
    db.create_meta(meta);

    VfsEngine vfs(db, cfg);
    vfs.reload_all_storages();
    TaskManager tasks(vfs, 2);
    ApiRouter router(cfg, db, vfs, tasks);

    HttpServer server("127.0.0.1", 0, 4, 256);
    server.set_stream_predicate([&](const HttpRequest& r) { return router.should_stream_request_body(r); });
    server.set_handler([&](HttpRequest& req, HttpResponse& resp) { router.dispatch(req, resp); });
    CHECK(server.start());
    int port = server.bound_port();
    CHECK(port > 0);

    ScopeExit stop_srv([&] {
        server.stop();
        db.close();
    });

    // A. Anti-Smuggling Check: Content-Length + Transfer-Encoding MUST return 400!
    auto smuggle = raw_http_call(port,
        "POST /api/fs/list HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: 4\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\n0\r\n\r\n");
    CHECK(smuggle.status == 400);

    // Overflow-sized chunk lengths must fail closed rather than wrap size_t.
    auto oversized_chunk = raw_http_call(port,
        "POST /api/fs/list HTTP/1.1\r\nHost: 127.0.0.1\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\nFFFFFFFFFFFFFFFFFFFFFFFF\r\n");
    CHECK(oversized_chunk.status == 413);

    // Chunk trailers must be fully consumed before the request is considered drained.
    auto chunk_trailer = raw_http_call(port,
        "POST /api/fs/list HTTP/1.1\r\nHost: 127.0.0.1\r\nTransfer-Encoding: chunked\r\nConnection: keep-alive\r\n\r\n0\r\nX-Check: ok\r\n\r\n"
        "GET /ping HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n");
    const size_t next_response = chunk_trailer.body.find("HTTP/1.1 200 OK");
    auto chunk_trailer_json = JsonValue::parse(std::string_view(chunk_trailer.body).substr(0, next_response));
    CHECK(chunk_trailer.status == 200 && chunk_trailer_json && chunk_trailer_json->get("code").as_i64() == 401);
    CHECK(next_response != std::string::npos && chunk_trailer.body.find("pong", next_response) != std::string::npos);

    // B. Health & Gzip Embedded SPA
    auto ping = raw_http_call(port, "GET /ping HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n");
    CHECK(ping.status == 200 && ping.body == "pong");

    auto spa = raw_http_call(port, "GET / HTTP/1.1\r\nHost: 127.0.0.1\r\nAccept-Encoding: gzip\r\nConnection: close\r\n\r\n");
    CHECK(spa.status == 200 && spa.headers["content-encoding"] == "gzip");

    // C. Guest Disabled by Default -> /api/fs/list & /d/* MUST reject unauthenticated access!
    auto unauth_ls = raw_http_call(port, "POST /api/fs/list HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: 12\r\nConnection: close\r\n\r\n{\"path\":\"/\"}");
    CHECK(JsonValue::parse(unauth_ls.body)->get("code").as_i64() == 401);

    auto unauth_dl = raw_http_call(port, "GET /d/local/secret_vault/confidential.txt HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n");
    CHECK(unauth_dl.status == 401);

    // D. Admin Login -> Obtain JWT
    std::string login_body = R"({"username":"admin","password":"AdminPass#2026"})";
    auto login_res = raw_http_call(port,
        "POST /api/auth/login HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Type: application/json\r\nContent-Length: " +
        std::to_string(login_body.size()) + "\r\nConnection: close\r\n\r\n" + login_body);
    auto login_json = JsonValue::parse(login_res.body);
    CHECK(login_json && login_json->get("code").as_i64() == 200);
    std::string admin_jwt = login_json->get("data").get("token").as_str();
    CHECK(!admin_jwt.empty());

    // The streaming chunk parser must reject overflowing lengths and leave no partial file.
    auto oversized_upload = raw_http_call(port,
        "PUT /api/fs/put HTTP/1.1\r\nHost: 127.0.0.1\r\nAuthorization: " + admin_jwt +
        "\r\nFile-Path: /local/alice_home/overflow.bin\r\nTransfer-Encoding: chunked\r\nConnection: close\r\n\r\nFFFFFFFFFFFFFFFFFFFFFFFF\r\n");
    auto oversized_upload_json = JsonValue::parse(oversized_upload.body);
    CHECK(oversized_upload_json && oversized_upload_json->get("code").as_i64() != 200);
    CHECK(!std::filesystem::exists(dir + "/storage/alice_home/overflow.bin"));

    // E. Create confined non-admin user "alice" restricted to /local/alice_home
    std::string create_alice = R"({"username":"alice","password":"AlicePassword1!","base_path":"/local/alice_home","role_ids":[2]})";
    auto ca_res = raw_http_call(port,
        "POST /api/admin/user/create HTTP/1.1\r\nHost: 127.0.0.1\r\nAuthorization: " + admin_jwt +
        "\r\nContent-Length: " + std::to_string(create_alice.size()) + "\r\nConnection: close\r\n\r\n" + create_alice);
    CHECK(JsonValue::parse(ca_res.body)->get("code").as_i64() == 200);

    std::string alice_login = R"({"username":"alice","password":"AlicePassword1!"})";
    auto al_res = raw_http_call(port,
        "POST /api/auth/login HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: " +
        std::to_string(alice_login.size()) + "\r\nConnection: close\r\n\r\n" + alice_login);
    std::string alice_jwt = JsonValue::parse(al_res.body)->get("data").get("token").as_str();
    CHECK(!alice_jwt.empty());

    // Verify alice CANNOT access /api/admin/* (Privilege Escalation Blocked!)
    auto alice_admin = raw_http_call(port,
        "GET /api/admin/user/list HTTP/1.1\r\nHost: 127.0.0.1\r\nAuthorization: " + alice_jwt + "\r\nConnection: close\r\n\r\n");
    CHECK(JsonValue::parse(alice_admin.body)->get("code").as_i64() == 403);

    // Verify alice CANNOT access /local/secret_vault via /d/* even with her JWT!
    auto alice_escape = raw_http_call(port,
        "GET /d/local/secret_vault/confidential.txt HTTP/1.1\r\nHost: 127.0.0.1\r\nAuthorization: " + alice_jwt + "\r\nConnection: close\r\n\r\n");
    CHECK(alice_escape.status == 403);

    // F. Anti-XSS Check on Active HTML Download + Range 206 + 416
    auto xss_dl = raw_http_call(port,
        "GET /d/local/alice_home/xss.html HTTP/1.1\r\nHost: 127.0.0.1\r\nAuthorization: " + alice_jwt + "\r\nConnection: close\r\n\r\n");
    CHECK(xss_dl.status == 200);
    CHECK(xss_dl.headers["content-type"] == "application/octet-stream");
    CHECK(xss_dl.headers["content-security-policy"].find("sandbox") != std::string::npos);

    auto range_dl = raw_http_call(port,
        "GET /d/local/alice_home/xss.html HTTP/1.1\r\nHost: 127.0.0.1\r\nAuthorization: " + alice_jwt +
        "\r\nRange: bytes=0-7\r\nConnection: close\r\n\r\n");
    CHECK(range_dl.status == 206 && range_dl.body == "<script>");

    auto bad_range = raw_http_call(port,
        "GET /d/local/alice_home/xss.html HTTP/1.1\r\nHost: 127.0.0.1\r\nAuthorization: " + alice_jwt +
        "\r\nRange: bytes=99999-100000\r\nConnection: close\r\n\r\n");
    CHECK(bad_range.status == 416);

    // G. Public Share with Password + Atomic max_downloads=2 Quota!
    std::string sh_req = R"({"path":"/local/secret_vault/confidential.txt","password":"SharePin77","max_downloads":2})";
    auto sh_create = raw_http_call(port,
        "POST /api/share/create HTTP/1.1\r\nHost: 127.0.0.1\r\nAuthorization: " + admin_jwt +
        "\r\nContent-Length: " + std::to_string(sh_req.size()) + "\r\nConnection: close\r\n\r\n" + sh_req);
    std::string share_id = JsonValue::parse(sh_create.body)->get("data").get("share_id").as_str();
    CHECK(!share_id.empty());

    CHECK(raw_http_call(port, "GET /sd/" + share_id + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n").status == 401);

    std::string sa_body = JsonValue::object({{"share_id", share_id}, {"password", "SharePin77"}}).dump();
    auto sa_res = raw_http_call(port,
        "POST /api/public/share/auth HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: " +
        std::to_string(sa_body.size()) + "\r\nConnection: close\r\n\r\n" + sa_body);
    std::string sh_tok = JsonValue::parse(sa_res.body)->get("data").get("token").as_str();
    CHECK(!sh_tok.empty());

    CHECK(raw_http_call(port, "GET /sd/" + share_id + "?token=" + url_encode(sh_tok) + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n").status == 200);
    CHECK(raw_http_call(port, "GET /sd/" + share_id + "?token=" + url_encode(sh_tok) + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n").status == 200);
    CHECK(raw_http_call(port, "GET /sd/" + share_id + "?token=" + url_encode(sh_tok) + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n").status == 403);

    // H. RFC 4918 WebDAV (PROPFIND, MKCOL, Streaming PUT, GET)
    std::string basic_admin = "Basic " + base64_encode("admin:AdminPass#2026");
    auto dav_mkcol = raw_http_call(port,
        "MKCOL /dav/local/dav_folder HTTP/1.1\r\nHost: 127.0.0.1\r\nAuthorization: " + basic_admin + "\r\nConnection: close\r\n\r\n");
    CHECK(dav_mkcol.status == 201);

    std::string dav_payload = "WebDAV Zero-RAM Streaming Payload 2026!";
    auto dav_put = raw_http_call(port,
        "PUT /dav/local/dav_folder/note.txt HTTP/1.1\r\nHost: 127.0.0.1\r\nAuthorization: " + basic_admin +
        "\r\nContent-Length: " + std::to_string(dav_payload.size()) + "\r\nConnection: close\r\n\r\n" + dav_payload);
    CHECK(dav_put.status == 201);

    auto dav_prop = raw_http_call(port,
        "PROPFIND /dav/local/dav_folder HTTP/1.1\r\nHost: 127.0.0.1\r\nDepth: 1\r\nAuthorization: " + basic_admin + "\r\nConnection: close\r\n\r\n");
    CHECK(dav_prop.status == 207 && dav_prop.body.find("note.txt") != std::string::npos);

    auto dav_get = raw_http_call(port,
        "GET /dav/local/dav_folder/note.txt HTTP/1.1\r\nHost: 127.0.0.1\r\nAuthorization: " + basic_admin + "\r\nConnection: close\r\n\r\n");
    CHECK(dav_get.status == 200 && dav_get.body == dav_payload);

    // I. Multi-Threaded Concurrency Hammer (12 threads x 25 requests = 300 concurrent ops)
    raw_http_call(port, "POST /api/admin/index/build HTTP/1.1\r\nHost: 127.0.0.1\r\nAuthorization: " + admin_jwt + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");

    std::vector<std::thread> threads;
    std::atomic<int> ok_ops{0};
    for (int t = 0; t < 12; ++t) {
        threads.emplace_back([&, t] {
            for (int i = 0; i < 25; ++i) {
                if (i % 3 == 0) {
                    std::string b = R"({"path":"/local/dav_folder"})";
                    auto r = raw_http_call(port,
                        "POST /api/fs/list HTTP/1.1\r\nHost: 127.0.0.1\r\nAuthorization: " + admin_jwt +
                        "\r\nContent-Length: " + std::to_string(b.size()) + "\r\nConnection: close\r\n\r\n" + b);
                    if (r.status == 200) ok_ops.fetch_add(1);
                } else if (i % 3 == 1) {
                    std::string b = R"({"parent":"/","keywords":"note"})";
                    auto r = raw_http_call(port,
                        "POST /api/fs/search HTTP/1.1\r\nHost: 127.0.0.1\r\nAuthorization: " + admin_jwt +
                        "\r\nContent-Length: " + std::to_string(b.size()) + "\r\nConnection: close\r\n\r\n" + b);
                    if (r.status == 200) ok_ops.fetch_add(1);
                } else {
                    std::string p = "Thread-" + std::to_string(t) + "-Iter-" + std::to_string(i);
                    auto r = raw_http_call(port,
                        "PUT /api/fs/put HTTP/1.1\r\nHost: 127.0.0.1\r\nAuthorization: " + admin_jwt +
                        "\r\nFile-Path: /local/dav_folder/t_" + std::to_string(t) + ".txt\r\nContent-Length: " +
                        std::to_string(p.size()) + "\r\nConnection: close\r\n\r\n" + p);
                    if (r.status == 200) ok_ops.fetch_add(1);
                }
            }
        });
    }
    for (auto& th : threads) th.join();
    CHECK(ok_ops.load() == 300);

    // J. Logout Token JTI Revocation -> Immediate 401 on subsequent reuse!
    auto lo = raw_http_call(port,
        "POST /api/auth/logout HTTP/1.1\r\nHost: 127.0.0.1\r\nAuthorization: " + alice_jwt + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    CHECK(JsonValue::parse(lo.body)->get("code").as_i64() == 200);

    auto reuse = raw_http_call(port,
        "GET /api/me HTTP/1.1\r\nHost: 127.0.0.1\r\nAuthorization: " + alice_jwt + "\r\nConnection: close\r\n\r\n");
    CHECK(JsonValue::parse(reuse.body)->get("code").as_i64() == 401);
}

} // namespace

int main() {
    std::signal(SIGPIPE, SIG_IGN);
    std::printf("\n=== AList-Core Standalone Security, Logic & Concurrency Suite ===\n");
    RUN_TEST(test_vpath_and_symlink_security);
    RUN_TEST(test_crypto_jwt_and_totp_security);
    RUN_TEST(test_ssrf_and_antiredos_glob);
    RUN_TEST(test_json_and_xml_bounds);
    RUN_TEST(test_database_vault_and_fts5);
    RUN_TEST(test_vfs_crypt_driver_and_loop_guard);
    RUN_TEST(test_zip_archive_security);
    RUN_TEST(test_live_http_api_webdav_and_concurrency);
    std::printf("=== ALL %d SUITES (%d ASSERTIONS) PASSED CLEANLY ===\n\n", g_tests_run, g_assertions.load());
    return 0;
}
