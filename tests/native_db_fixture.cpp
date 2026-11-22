// Native database integration fixture. Only test credentials and temporary data.
#include "alist/db.hpp"
#include "alist/crypto.hpp"
#include <string>

int main(int argc, char** argv) {
    if (argc != 3) return 2;
    const std::string mode = argv[1];
    alist::Database db;
    if (!db.init(argv[2], "fixture-master-secret")) return 3;
    if (mode == "create") {
        alist::UserRecord user;
        user.username = "interop-user";
        user.password_hash = alist::hash_password_pbkdf2("fixture-password", 10000);
        user.base_path = "/home/alice";
        user.role_ids = {12, 42};
        user.otp_secret = "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ";
        user.last_otp_step = 5;
        auto id = db.create_user(user);
        if (!id) return 4;
        alist::StorageRecord s;
        s.mount_path = "/interop";
        s.driver = "Local";
        s.addition = R"({"root_folder_path":"/fixture/private"})";
        s.enable_sign = true;
        if (!db.create_storage(s)) return 5;
        alist::ShareRecord share;
        share.share_id = "interop-share";
        share.creator_id = id;
        share.creator_name = user.username;
        share.path = "/interop/file";
        share.max_downloads = 7;
        if (!db.create_share(share)) return 6;
        if (!db.set_setting("interop-key", "interop-value", "site", true)) return 7;
        alist::FileObj file;
        file.name = "Architecture.pdf";
        file.size = 42;
        db.index_directory_entries("/interop", {file});
        return 0;
    }
    if (mode == "inspect-search") {
        auto [hits, total] = db.search_files("blueprint", "/interop", 0, 1, 20);
        return (total == 1 && hits.size() == 1 && hits[0].name == "Blueprint.txt" &&
                hits[0].parent == "/interop/nested" && hits[0].size == 101) ? 0 : 9;
    }
    if (mode == "inspect") {
        auto user = db.get_user_by_name("interop-user");
        auto share = db.get_share("interop-share");
        if (!user || user->last_otp_step != 40 || !share || share->downloads != 7) return 8;
        return 0;
    }
    return 2;
}
