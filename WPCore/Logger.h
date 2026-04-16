#pragma once

#include <array>
#include <string>
#include <mutex>
#include <chrono>
#include <vector>

class Logger
{
public:
    static constexpr std::size_t MAXLOGS = 50;

    enum class Level
    {
        LV_INFO,
        LV_ERROR,
    };

    struct LogEntry
    {
        Level level;
        std::chrono::system_clock::time_point timestamp;
        std::string message;
    };

    static Logger& instance();

    void info(std::string msg);
    void error(std::string msg);

    std::vector<LogEntry> snapshot() const;
    //std::vector<std::wstring> formattedSnapshot() const;

    void clear();

private:
    Logger() = default;

    void log(Level level, std::string msg);

    mutable std::mutex mutex_;
    std::array<LogEntry, MAXLOGS> buffer_{};
    std::size_t write_index_{ 0 };
    std::size_t size_{ 0 };
};
