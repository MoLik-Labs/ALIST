#pragma once

#include <atomic>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

namespace alist {

enum class LogLevel : int {
    Debug = 0,
    Info  = 1,
    Warn  = 2,
    Error = 3,
};

class Logger {
public:
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    void set_level(LogLevel lvl) noexcept { level_.store(lvl, std::memory_order_relaxed); }
    LogLevel level() const noexcept { return level_.load(std::memory_order_relaxed); }

    void set_file(const std::string& path) {
        std::lock_guard<std::mutex> lk(mu_);
        if (file_.is_open()) file_.close();
        if (!path.empty()) {
            file_.open(path, std::ios::out | std::ios::app);
        }
    }

    void log(LogLevel lvl, std::string_view component, std::string_view msg) {
        if (static_cast<int>(lvl) < static_cast<int>(level_.load(std::memory_order_relaxed))) {
            return;
        }
        auto now = std::chrono::system_clock::now();
        auto sec = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
        std::tm tm_utc{};
        gmtime_r(&sec, &tm_utc);

        const char* tag = "INFO";
        if (lvl == LogLevel::Debug) tag = "DEBUG";
        else if (lvl == LogLevel::Warn) tag = "WARN";
        else if (lvl == LogLevel::Error) tag = "ERROR";

        // Sanitize CR/LF in log message to defeat CRLF log injection attacks!
        std::string safe_msg;
        safe_msg.reserve(msg.size());
        for (char c : msg) {
            if (c == '\r') safe_msg += "\\r";
            else if (c == '\n') safe_msg += "\\n";
            else safe_msg.push_back(c);
        }

        char ts[96];
        std::snprintf(ts, sizeof(ts), "%04d-%02d-%02dT%02d:%02d:%02d.%03lldZ",
                      tm_utc.tm_year + 1900, tm_utc.tm_mon + 1, tm_utc.tm_mday,
                      tm_utc.tm_hour, tm_utc.tm_min, tm_utc.tm_sec,
                      static_cast<long long>(ms));

        std::lock_guard<std::mutex> lk(mu_);
        std::fprintf(stderr, "[%s] [%-5s] [%.*s] %s\n",
                     ts, tag, static_cast<int>(component.size()), component.data(), safe_msg.c_str());
        if (file_.is_open()) {
            file_ << "[" << ts << "] [" << tag << "] [" << component << "] " << safe_msg << "\n";
            file_.flush();
        }
    }

private:
    Logger() = default;
    std::atomic<LogLevel> level_{LogLevel::Info};
    std::mutex mu_;
    std::ofstream file_;
};

#define LOG_DEBUG(comp, msg) ::alist::Logger::instance().log(::alist::LogLevel::Debug, (comp), (msg))
#define LOG_INFO(comp, msg)  ::alist::Logger::instance().log(::alist::LogLevel::Info,  (comp), (msg))
#define LOG_WARN(comp, msg)  ::alist::Logger::instance().log(::alist::LogLevel::Warn,  (comp), (msg))
#define LOG_ERROR(comp, msg) ::alist::Logger::instance().log(::alist::LogLevel::Error, (comp), (msg))

} // namespace alist
