#include "alist/tasks.hpp"
#include "alist/common.hpp"
#include "alist/crypto.hpp"
#include "alist/http_client.hpp"
#include "alist/security.hpp"

#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace alist {

namespace {
constexpr size_t kMaxActiveTasks = 256;
constexpr size_t kMaxTaskHistory = 2000;
} // namespace

TaskManager::TaskManager(VfsEngine& vfs, int num_workers) : vfs_(vfs) {
    int n = std::clamp(num_workers, 1, 16);
    try {
        for (int i = 0; i < n; ++i) {
            workers_.emplace_back([this] { worker_loop(); });
        }
    } catch (...) {
        {
            std::lock_guard<std::mutex> lk(mu_);
            stopping_ = true;
        }
        cv_.notify_all();
        for (auto& worker : workers_) {
            if (worker.joinable()) worker.join();
        }
        throw;
    }
}

TaskManager::~TaskManager() {
    {
        std::lock_guard<std::mutex> lk(mu_);
        stopping_ = true;
        const int64_t now = unix_now();
        for (auto& [_, t] : all_tasks_) {
            t->cancel_flag.store(true, std::memory_order_relaxed);
            if (t->rec.state == "pending") {
                t->rec.state = "canceled";
                t->rec.status_text = "canceled during shutdown";
                t->rec.updated_at = now;
            }
        }
        queue_.clear();
    }
    cv_.notify_all();
    for (auto& th : workers_) {
        if (th.joinable()) th.join();
    }
}

size_t TaskManager::count_active_unlocked() const noexcept {
    size_t active = 0;
    for (const auto& [_, existing] : all_tasks_) {
        if (existing->rec.state == "pending" || existing->rec.state == "running") ++active;
    }
    return active;
}

void TaskManager::worker_loop() {
    while (true) {
        std::shared_ptr<InternalTask> task;
        {
            std::unique_lock<std::mutex> lk(mu_);
            cv_.wait(lk, [&] { return stopping_ || !queue_.empty(); });
            if (stopping_ && queue_.empty()) return;
            task = std::move(queue_.front());
            queue_.pop_front();
            if (task->cancel_flag.load(std::memory_order_relaxed)) {
                task->rec.state = "canceled";
                task->rec.status_text = "canceled";
                task->rec.updated_at = unix_now();
                continue;
            }
            task->rec.state = "running";
            task->rec.status_text = "executing";
            task->rec.updated_at = unix_now();
        }

        std::string err;
        bool ok = false;
        try {
            ok = task->runner(*task, err);
        } catch (const std::exception& ex) {
            err = ex.what();
        }

        {
            std::lock_guard<std::mutex> lk(mu_);
            task->rec.updated_at = unix_now();
            if (task->cancel_flag.load(std::memory_order_relaxed)) {
                task->rec.state = "canceled";
                task->rec.status_text = "canceled";
            } else if (ok) {
                task->rec.state = "succeeded";
                task->rec.status_text = "completed";
                task->rec.progress = 100.0;
            } else {
                task->rec.state = "failed";
                task->rec.status_text = "error";
                task->rec.error_msg = std::move(err);
            }
        }
    }
}

std::string TaskManager::enqueue_copy(std::string creator, std::string src_vpath, std::string dst_dir_vpath) {
    auto t = std::make_shared<InternalTask>();
    t->rec.id = "tsk_" + random_hex(8);
    t->rec.type = "copy";
    t->rec.name = "Copy " + src_vpath + " -> " + dst_dir_vpath;
    t->rec.creator = std::move(creator);
    t->rec.state = "pending";
    t->rec.status_text = "queued";
    t->rec.created_at = unix_now();
    t->rec.updated_at = t->rec.created_at;

    t->runner = [this, src = std::move(src_vpath), dst = std::move(dst_dir_vpath)](InternalTask& self, std::string& err) -> bool {
        if (self.cancel_flag.load(std::memory_order_relaxed)) return false;
        return vfs_.copy_obj_sync(src, dst, err);
    };

    std::string id = t->rec.id;
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (count_active_unlocked() >= kMaxActiveTasks) return {};
        if (all_tasks_.size() >= kMaxTaskHistory) clear_done_tasks_unlocked("");
        queue_.push_back(t);
        try {
            const auto inserted = all_tasks_.emplace(id, t).second;
            if (!inserted) {
                queue_.pop_back();
                return {};
            }
        } catch (...) {
            queue_.pop_back();
            throw;
        }
    }
    cv_.notify_one();
    return id;
}

std::string TaskManager::enqueue_offline_download(std::string creator, std::string url, std::string dst_dir_vpath) {
    auto t = std::make_shared<InternalTask>();
    t->rec.id = "tsk_" + random_hex(8);
    t->rec.type = "offline_download";
    t->rec.name = "Download " + url + " -> " + dst_dir_vpath;
    t->rec.creator = std::move(creator);
    t->rec.state = "pending";
    t->rec.status_text = "queued";
    t->rec.created_at = unix_now();
    t->rec.updated_at = t->rec.created_at;

    t->runner = [this, dl_url = std::move(url), dst_dir = std::move(dst_dir_vpath)](InternalTask& self, std::string& err) -> bool {
        auto validated = validate_outbound_url(dl_url, vfs_.config().allow_private_net_outbound, &err);
        if (!validated) return false;

        std::string_view p = validated->path_and_query;
        auto qmark = p.find('?');
        if (qmark != std::string_view::npos) p = p.substr(0, qmark);
        auto [_, raw_base] = split_vpath(p);
        std::string fname = url_decode(raw_base).value_or("");
        if (!is_valid_filename(fname)) {
            fname = "download_" + random_hex(4) + ".bin";
        }

        std::string spool = vfs_.config().temp_dir + "/offdl_" + random_hex(8) + ".tmp";
        int flags = O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC;
#ifdef O_NOFOLLOW
        flags |= O_NOFOLLOW;
#endif
        UniqueFd wfd(::open(spool.c_str(), flags, 0600));
        if (!wfd) { err = "Failed creating temp file"; return false; }
        ScopeExit rm_spool([&] { ::unlink(spool.c_str()); });

        int64_t expected_len = 0;
        int64_t downloaded = 0;
        OutboundRequest req;
        req.url = dl_url;
        req.allow_private_net = vfs_.config().allow_private_net_outbound;
        req.follow_redirects = true; // Hop-by-hop SSRF-validated inside HttpClient::perform
        req.timeout_ms = 1800000;
        req.should_abort = [&] { return self.cancel_flag.load(std::memory_order_relaxed); };
        req.on_headers = [&](int, const std::map<std::string, std::string>& hdrs) {
            if (auto it = hdrs.find("content-length"); it != hdrs.end()) {
                expected_len = std::max<int64_t>(0, parse_i64(it->second).value_or(0));
            }
        };
        req.on_data = [&](const char* d, size_t n) -> bool {
            if (self.cancel_flag.load(std::memory_order_relaxed)) return false;
            const int64_t limit = std::max<int64_t>(0, vfs_.config().max_upload_bytes);
            if (downloaded < 0 || downloaded > limit ||
                n > static_cast<uint64_t>(limit - downloaded)) {
                err = "Offline download exceeded max_upload_bytes limit";
                return false;
            }
            size_t written = 0;
            while (written < n) {
                ssize_t amount = ::write(wfd.get(), d + written, n - written);
                if (amount < 0 && errno == EINTR) continue;
                if (amount <= 0) {
                    err = "Failed writing offline download to temporary file";
                    return false;
                }
                written += static_cast<size_t>(amount);
            }
            downloaded += static_cast<int64_t>(n);
            if (expected_len > 0) {
                std::lock_guard<std::mutex> lk(mu_);
                self.rec.progress = std::min(95.0, (static_cast<double>(downloaded) * 95.0) / static_cast<double>(expected_len));
            }
            return true;
        };
        auto resp = HttpClient::instance().perform(req);
        if (!resp.ok || resp.status_code < 200 || resp.status_code >= 300) {
            if (err.empty()) err = resp.error.empty() ? ("HTTP " + std::to_string(resp.status_code)) : resp.error;
            return false;
        }

        if (::lseek(wfd.get(), 0, SEEK_SET) < 0) {
            err = "Failed rewinding temporary download file";
            return false;
        }
        auto dst_full = join_vpath(dst_dir, fname);
        if (!dst_full) { err = "Invalid destination path"; return false; }

        UploadStream up;
        up.filename = fname;
        up.size = downloaded;
        up.mime_type = std::string(guess_mime_type(fname));
        up.read_fn = [&](char* buf, size_t max_len) -> ssize_t {
            if (self.cancel_flag.load(std::memory_order_relaxed)) return -1;
            while (true) {
                ssize_t n = ::read(wfd.get(), buf, max_len);
                if (n < 0 && errno == EINTR) continue;
                return n;
            }
        };
        return vfs_.put_stream(*dst_full, up, err);
    };

    std::string id = t->rec.id;
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (count_active_unlocked() >= kMaxActiveTasks) return {};
        if (all_tasks_.size() >= kMaxTaskHistory) clear_done_tasks_unlocked("");
        queue_.push_back(t);
        try {
            const auto inserted = all_tasks_.emplace(id, t).second;
            if (!inserted) {
                queue_.pop_back();
                return {};
            }
        } catch (...) {
            queue_.pop_back();
            throw;
        }
    }
    cv_.notify_one();
    return id;
}

std::vector<TaskRecord> TaskManager::list_tasks(std::string_view type_filter, bool done_only) const {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<TaskRecord> out;
    out.reserve(all_tasks_.size());
    for (const auto& [_, t] : all_tasks_) {
        if (!type_filter.empty() && t->rec.type != type_filter) continue;
        bool is_done = (t->rec.state == "succeeded" || t->rec.state == "failed" || t->rec.state == "canceled");
        if (done_only != is_done) continue;
        out.push_back(t->rec);
    }
    std::sort(out.begin(), out.end(), [](const TaskRecord& a, const TaskRecord& b) {
        return a.created_at > b.created_at;
    });
    return out;
}

bool TaskManager::cancel_task(std::string_view task_id, std::string_view owner_filter) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = all_tasks_.find(std::string(task_id));
    if (it == all_tasks_.end()) return false;
    if (!owner_filter.empty() && it->second->rec.creator != owner_filter) return false;
    it->second->cancel_flag.store(true, std::memory_order_relaxed);
    if (it->second->rec.state == "pending") {
        const std::string_view canceled_id = it->second->rec.id;
        queue_.erase(std::remove_if(queue_.begin(), queue_.end(), [&](const auto& queued) {
            return std::string_view(queued->rec.id) == canceled_id;
        }), queue_.end());
        it->second->rec.state = "canceled";
        it->second->rec.status_text = "canceled";
        it->second->rec.updated_at = unix_now();
    }
    return true;
}

void TaskManager::clear_done_tasks(std::string_view type_filter, std::string_view owner_filter) {
    std::lock_guard<std::mutex> lk(mu_);
    clear_done_tasks_unlocked(type_filter, owner_filter);
}

void TaskManager::clear_done_tasks_unlocked(std::string_view type_filter, std::string_view owner_filter) {
    for (auto it = all_tasks_.begin(); it != all_tasks_.end();) {
        const auto& rec = it->second->rec;
        bool is_done = (rec.state == "succeeded" || rec.state == "failed" || rec.state == "canceled");
        bool type_ok = type_filter.empty() || rec.type == type_filter;
        bool owner_ok = owner_filter.empty() || rec.creator == owner_filter;
        if (is_done && type_ok && owner_ok) {
            it = all_tasks_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace alist
