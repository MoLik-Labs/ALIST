#pragma once

#include "alist/models.hpp"
#include "alist/vfs.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace alist {

class TaskManager {
public:
    explicit TaskManager(VfsEngine& vfs, int num_workers = 4);
    ~TaskManager();

    TaskManager(const TaskManager&) = delete;
    TaskManager& operator=(const TaskManager&) = delete;

    std::string enqueue_copy(std::string creator, std::string src_vpath, std::string dst_dir_vpath);
    std::string enqueue_offline_download(std::string creator, std::string url, std::string dst_dir_vpath);

    std::vector<TaskRecord> list_tasks(std::string_view type_filter, bool done_only) const;
    bool cancel_task(std::string_view task_id);
    void clear_done_tasks(std::string_view type_filter);

private:
    struct InternalTask {
        TaskRecord rec;
        std::atomic<bool> cancel_flag{false};
        std::function<bool(InternalTask&, std::string&)> runner;
    };

    void worker_loop();
    void clear_done_tasks_unlocked(std::string_view type_filter);

    VfsEngine& vfs_;
    mutable std::mutex mu_;
    std::condition_variable cv_;
    bool stopping_ = false;
    std::deque<std::shared_ptr<InternalTask>> queue_;
    std::unordered_map<std::string, std::shared_ptr<InternalTask>> all_tasks_;
    std::vector<std::thread> workers_;
};

} // namespace alist
