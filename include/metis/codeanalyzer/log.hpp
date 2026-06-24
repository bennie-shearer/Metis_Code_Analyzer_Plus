/* Metis Code Analyzer Plus - logging subsystem
 * Thread-safe in-memory ring buffer plus console echo. Levels, timestamps,
 * retrieval, and clear. 7-bit ASCII, C++20.
 */
#ifndef METIS_CODE_ANALYZER_LOG_HPP
#define METIS_CODE_ANALYZER_LOG_HPP

#include <chrono>
#include <ctime>
#include <deque>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

namespace metis::codeanalyzer {

enum class LogLevel { Debug, Info, Warn, Error };

inline const char* log_level_name(LogLevel l) {
    switch (l) {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info:  return "INFO";
    case LogLevel::Warn:  return "WARN";
    case LogLevel::Error: return "ERROR";
    }
    return "INFO";
}

inline LogLevel log_level_from(const std::string& s) {
    if (s == "debug" || s == "DEBUG") return LogLevel::Debug;
    if (s == "warn"  || s == "WARN")  return LogLevel::Warn;
    if (s == "error" || s == "ERROR") return LogLevel::Error;
    return LogLevel::Info;
}

struct LogEntry {
    std::string timestamp;   /* ISO-8601 UTC */
    LogLevel level;
    std::string message;
};

class Logger {
public:
    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    void configure(LogLevel min_level, std::size_t max_entries) {
        std::lock_guard<std::mutex> lk(mtx_);
        min_level_ = min_level;
        max_entries_ = max_entries == 0 ? 1 : max_entries;
    }

    void log(LogLevel level, const std::string& msg) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (static_cast<int>(level) < static_cast<int>(min_level_)) return;
        LogEntry e{now_iso8601(), level, msg};
        std::cerr << "[metis-code-analyzer] " << e.timestamp << " "
                  << log_level_name(level) << " " << msg << "\n";
        entries_.push_back(std::move(e));
        while (entries_.size() > max_entries_) entries_.pop_front();
    }

    void debug(const std::string& m) { log(LogLevel::Debug, m); }
    void info(const std::string& m)  { log(LogLevel::Info, m); }
    void warn(const std::string& m)  { log(LogLevel::Warn, m); }
    void error(const std::string& m) { log(LogLevel::Error, m); }

    std::vector<LogEntry> recent(std::size_t limit) const {
        std::lock_guard<std::mutex> lk(mtx_);
        std::vector<LogEntry> out;
        std::size_t start = (limit == 0 || limit >= entries_.size())
                                ? 0 : entries_.size() - limit;
        for (std::size_t i = start; i < entries_.size(); ++i) out.push_back(entries_[i]);
        return out;
    }

    std::size_t clear() {
        std::lock_guard<std::mutex> lk(mtx_);
        std::size_t n = entries_.size();
        entries_.clear();
        return n;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lk(mtx_);
        return entries_.size();
    }

private:
    Logger() = default;

    static std::string now_iso8601() {
        using namespace std::chrono;
        std::time_t t = system_clock::to_time_t(system_clock::now());
        std::tm tm_utc{};
#ifdef _WIN32
        gmtime_s(&tm_utc, &t);
#else
        gmtime_r(&t, &tm_utc);
#endif
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
        return std::string(buf);
    }

    mutable std::mutex mtx_;
    std::deque<LogEntry> entries_;
    LogLevel min_level_ = LogLevel::Info;
    std::size_t max_entries_ = 1000;
};

} /* namespace metis::codeanalyzer */

#endif /* METIS_CODE_ANALYZER_LOG_HPP */
