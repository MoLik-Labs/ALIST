#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

namespace alist {

struct TransparentStringHash {
    using is_transparent = void;
    [[nodiscard]] size_t operator()(std::string_view sv) const noexcept {
        return std::hash<std::string_view>{}(sv);
    }
    [[nodiscard]] size_t operator()(const std::string& s) const noexcept {
        return std::hash<std::string_view>{}(s);
    }
    [[nodiscard]] size_t operator()(const char* s) const noexcept {
        return std::hash<std::string_view>{}(s);
    }
};

// High-concurrency 64-shard bounded LRU+TTL cache with zero-allocation heterogeneous lookup.
// Stores immutable shared_ptr snapshots for collection values so readers never race with writers.
template <typename V, size_t Shards = 64>
class ShardedLruCache {
public:
    explicit ShardedLruCache(size_t max_entries_per_shard = 256)
        : max_per_shard_(max_entries_per_shard < 4 ? 4 : max_entries_per_shard) {}

    std::optional<V> get(std::string_view key) {
        auto& sh = shard_for(key);
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lk(sh.mu);
        auto it = sh.index.find(key);
        if (it == sh.index.end()) {
            ++sh.misses;
            return std::nullopt;
        }
        if (it->second->expires_at <= now) {
            sh.lru.erase(it->second);
            sh.index.erase(it);
            ++sh.misses;
            return std::nullopt;
        }
        sh.lru.splice(sh.lru.begin(), sh.lru, it->second);
        ++sh.hits;
        return it->second->value;
    }

    void put(std::string_view key, V value, std::chrono::seconds ttl) {
        if (ttl.count() <= 0) return;
        auto& sh = shard_for(key);
        auto exp = std::chrono::steady_clock::now() + ttl;
        std::lock_guard<std::mutex> lk(sh.mu);
        auto it = sh.index.find(key);
        if (it != sh.index.end()) {
            it->second->value = std::move(value);
            it->second->expires_at = exp;
            sh.lru.splice(sh.lru.begin(), sh.lru, it->second);
            return;
        }
        if (sh.lru.size() >= max_per_shard_) {
            sh.index.erase(sh.lru.back().key);
            sh.lru.pop_back();
        }
        std::string k(key);
        sh.lru.push_front(Node{k, std::move(value), exp});
        sh.index.emplace(std::move(k), sh.lru.begin());
    }

    void erase(std::string_view key) {
        auto& sh = shard_for(key);
        std::lock_guard<std::mutex> lk(sh.mu);
        auto it = sh.index.find(key);
        if (it != sh.index.end()) {
            sh.lru.erase(it->second);
            sh.index.erase(it);
        }
    }

    void erase_prefix(std::string_view prefix) {
        for (auto& sh : shards_) {
            std::lock_guard<std::mutex> lk(sh.mu);
            for (auto it = sh.lru.begin(); it != sh.lru.end();) {
                if (it->key == prefix ||
                    (it->key.size() > prefix.size() &&
                     it->key.starts_with(prefix) &&
                     (prefix == "/" || it->key[prefix.size()] == '/' || it->key[prefix.size()] == '\0'))) {
                    sh.index.erase(it->key);
                    it = sh.lru.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    void clear() {
        for (auto& sh : shards_) {
            std::lock_guard<std::mutex> lk(sh.mu);
            sh.index.clear();
            sh.lru.clear();
        }
    }

    [[nodiscard]] size_t size() const {
        size_t total = 0;
        for (auto& sh : shards_) {
            std::lock_guard<std::mutex> lk(sh.mu);
            total += sh.lru.size();
        }
        return total;
    }

    [[nodiscard]] uint64_t hits() const noexcept {
        uint64_t total = 0;
        for (const auto& sh : shards_) {
            std::lock_guard<std::mutex> lk(sh.mu);
            total += sh.hits;
        }
        return total;
    }
    [[nodiscard]] uint64_t misses() const noexcept {
        uint64_t total = 0;
        for (const auto& sh : shards_) {
            std::lock_guard<std::mutex> lk(sh.mu);
            total += sh.misses;
        }
        return total;
    }

private:
    struct Node {
        std::string key;
        V value;
        std::chrono::steady_clock::time_point expires_at;
    };
    struct alignas(64) Shard {
        mutable std::mutex mu;
        std::list<Node> lru;
        std::unordered_map<std::string, typename std::list<Node>::iterator, TransparentStringHash, std::equal_to<>> index;
        uint64_t hits = 0;
        uint64_t misses = 0;
    };

    Shard& shard_for(std::string_view key) noexcept {
        size_t h = std::hash<std::string_view>{}(key);
        return shards_[h % Shards];
    }

    std::array<Shard, Shards> shards_;
    size_t max_per_shard_;
};

// Coalesces concurrent identical requests into a single in-flight execution (prevents cache stampede)
template <typename R>
class Singleflight {
public:
    template <typename Fn>
    R execute(const std::string& key, Fn&& fn) {
        std::shared_ptr<Call> call;
        bool is_leader = false;
        {
            std::unique_lock<std::mutex> lk(mu_);
            auto it = calls_.find(key);
            if (it != calls_.end()) {
                call = it->second;
            } else {
                call = std::make_shared<Call>();
                call->owner = std::this_thread::get_id();
                calls_.emplace(key, call);
                is_leader = true;
            }
        }

        if (!is_leader) {
            if (call->owner == std::this_thread::get_id()) {
                return R{};
            }
            std::unique_lock<std::mutex> clk(call->mu);
            call->cv.wait(clk, [&] { return call->done; });
            return call->result;
        }

        try {
            R res = fn();
            {
                std::lock_guard<std::mutex> clk(call->mu);
                call->result = res;
                call->done = true;
            }
            call->cv.notify_all();
            {
                std::lock_guard<std::mutex> lk(mu_);
                calls_.erase(key);
            }
            return res;
        } catch (...) {
            {
                std::lock_guard<std::mutex> clk(call->mu);
                call->done = true;
            }
            call->cv.notify_all();
            {
                std::lock_guard<std::mutex> lk(mu_);
                calls_.erase(key);
            }
            throw;
        }
    }

private:
    struct Call {
        std::mutex mu;
        std::condition_variable cv;
        std::thread::id owner{};
        bool done = false;
        R result{};
    };
    std::mutex mu_;
    std::unordered_map<std::string, std::shared_ptr<Call>> calls_;
};

} // namespace alist
