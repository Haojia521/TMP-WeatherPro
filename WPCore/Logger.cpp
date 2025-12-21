#include "Logger.h"

static std::wstring formatEntry(const Logger::LogEntry& e) {
    using namespace std::chrono;

    // time_point → 本地时间
    auto tp = floor<milliseconds>(e.timestamp);
    auto tt = system_clock::to_time_t(tp);
    auto ms = duration_cast<milliseconds>(tp.time_since_epoch()) % 1000;

    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif

    const wchar_t* level =
        (e.level == Logger::Level::INFO) ? L"INFO" : L"ERROR";

    return std::format(
        L"[{:%Y-%m-%d %H:%M:%S}.{:03d}][{}] {}",
        tm, ms.count(), level, e.message
    );
}

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::info(std::wstring msg) {
    log(Level::INFO, std::move(msg));
}

void Logger::error(std::wstring msg) {
    log(Level::ERROR, std::move(msg));
}

/// 获取当前所有日志（按时间顺序，从旧到新）
std::vector<Logger::LogEntry> Logger::snapshot() const {
    std::lock_guard lock(mutex_);

    std::vector<LogEntry> result;
    result.reserve(size_);

    // 计算最旧日志的位置
    std::size_t start =
        (size_ < MAXLOGS) ? 0 : write_index_;

    for (std::size_t i = 0; i < size_; ++i) {
        std::size_t idx = (start + i) % MAXLOGS;
        result.push_back(buffer_[idx]);
    }
    return result;
}

std::vector<std::wstring> Logger::formattedSnapshot() const {
    auto log_entries = snapshot();

    std::vector<std::wstring> log_lines;
    log_lines.reserve(log_entries.size());

    for (const auto &entry : log_entries) {
        log_lines.emplace_back(formatEntry(entry));
    }

    return log_lines;
}

void Logger::log(Level level, std::wstring msg) {
    std::lock_guard lock(mutex_);

    buffer_[write_index_] = LogEntry{
        level,
        std::chrono::system_clock::now(),
        std::move(msg)
    };

    write_index_ = (write_index_ + 1) % MAXLOGS;
    if (size_ < MAXLOGS) {
        ++size_;
    }
}

void Logger::clear() {
    std::lock_guard lock(mutex_);
    size_ = 0;
    write_index_ = 0;
}
