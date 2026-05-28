#include "Logger.h"

#include <thread>

namespace 
{
    std::string_view toString(Logger::Level lv) {
        switch (lv) {
        case Logger::Level::LV_INFO:
            return "INFO";

        case Logger::Level::LV_ERROR:
            return "ERROR";
        }

        return "UNKOWN";
    }

    std::string formatTime(std::chrono::system_clock::time_point tp)
    {
        using namespace std::chrono;

        auto sec = floor<seconds>(tp);
        auto ms = duration_cast<milliseconds>(tp - sec).count();

        zoned_time zt{ current_zone(), sec };

        return std::format("{:%F %T}.{:03}", zt, ms);
    }

    std::string formatLog(const Logger::LogEntry& log)
    {
        return std::format("[{}][{:X}][{}] {}",
                           formatTime(log.timestamp),
                           log.thread_id,
                           toString(log.level),
                           log.message
        );
    }
}

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::info(std::string_view msg) {
    log(Level::LV_INFO, std::string{ msg });
}

void Logger::error(std::string_view msg) {
    log(Level::LV_ERROR, std::string{ msg });
}

/// 获取当前所有日志（按时间顺序，从旧到新）
std::vector<Logger::LogEntry> Logger::snapshot() const {
    std::scoped_lock lock{ mutex_ };

    std::vector<LogEntry> result;
    result.reserve(size_);

    // 计算最旧日志的位置
    const std::size_t start = (size_ < MAXLOGS) ? 0 : write_index_;

    for (std::size_t i = 0; i < size_; ++i) {
        const std::size_t idx = (start + i) % MAXLOGS;
        result.push_back(buffer_[idx]);
    }
    return result;
}

std::vector<std::string> Logger::formattedSnapshot() const {
    const auto log_entries = snapshot();

    std::vector<std::string> log_lines;
    log_lines.reserve(log_entries.size());

    for (const auto &entry : log_entries) {
        log_lines.push_back(formatLog(entry));
    }

    return log_lines;
}

void Logger::log(Level level, std::string msg) {
    std::scoped_lock lock{ mutex_ };

    buffer_[write_index_] = LogEntry{
        .level = level,
        .timestamp = std::chrono::system_clock::now(),
        .thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id()) & 0xFFFFFF,
        .message = std::move(msg)
    };

    write_index_ = (write_index_ + 1) % MAXLOGS;
    if (size_ < MAXLOGS) {
        ++size_;
    }
}

void Logger::clear() {
    std::scoped_lock lock{ mutex_ };
    size_ = 0;
    write_index_ = 0;
}
