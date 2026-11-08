#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <format>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace c8 {

enum class LogLevel : uint8_t {
    debug,
    info,
    warning,
    error,
};

/// The name of a level, for example "Warning"
[[nodiscard]]
auto to_string(LogLevel level) noexcept -> std::string_view;


/// One log message
struct LogEntry {
    uint64_t    sequence = 0;   ///< 1 for the first message, then 1 more for each message
    double      seconds  = 0.0; ///< The time of the message, in seconds after the log was created
    LogLevel    level    = LogLevel::info;
    std::string message;
};


/**
 * @class Log
 *
 * @brief A thread-safe ring buffer of log messages
 *
 * @details When the buffer is full, a new message replaces the oldest one.
 *          Any thread can write. The emulator core does not write to the log.
 *          It returns errors to the frontend, and the frontend logs them.
 */
class Log {
public:
    static constexpr size_t default_capacity = 2000;

    /// A log that keeps the newest `capacity` messages (at least 1)
    explicit Log(size_t capacity = default_capacity);

    /// Add a message
    auto write(LogLevel level, std::string message) -> void;

    /// Remove all messages. The sequence numbers continue.
    auto clear() -> void;

    /// A copy of the messages, oldest first
    [[nodiscard]]
    auto entries() const -> std::vector<LogEntry>;

    /// The number of messages in the buffer
    [[nodiscard]]
    auto size() const -> size_t;

    [[nodiscard]]
    auto capacity() const noexcept -> size_t {
        return max_entries;
    }

    /// A counter that increases on each write and each clear. A reader copies the entries only when it changes.
    [[nodiscard]]
    auto version() const noexcept -> uint64_t {
        return change_count.load(std::memory_order_acquire);
    }

private:
    mutable std::mutex mutex;
    std::deque<LogEntry> buffer;
    size_t max_entries;
    uint64_t next_sequence = 1;
    std::atomic<uint64_t> change_count{0};
    std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
};


/// The log of the application
[[nodiscard]]
auto app_log() -> Log&;

template<typename... ArgsT>
auto log_debug(std::format_string<ArgsT...> format, ArgsT&&... args) -> void {
    app_log().write(LogLevel::debug, std::format(format, std::forward<ArgsT>(args)...));
}

template<typename... ArgsT>
auto log_info(std::format_string<ArgsT...> format, ArgsT&&... args) -> void {
    app_log().write(LogLevel::info, std::format(format, std::forward<ArgsT>(args)...));
}

template<typename... ArgsT>
auto log_warning(std::format_string<ArgsT...> format, ArgsT&&... args) -> void {
    app_log().write(LogLevel::warning, std::format(format, std::forward<ArgsT>(args)...));
}

template<typename... ArgsT>
auto log_error(std::format_string<ArgsT...> format, ArgsT&&... args) -> void {
    app_log().write(LogLevel::error, std::format(format, std::forward<ArgsT>(args)...));
}

} // namespace c8
