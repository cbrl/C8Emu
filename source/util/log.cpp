#include "log.h"

#include <algorithm>

namespace c8 {

auto to_string(LogLevel level) noexcept -> std::string_view {
    switch (level) {
        case LogLevel::debug:   return "Debug";
        case LogLevel::info:    return "Info";
        case LogLevel::warning: return "Warning";
        case LogLevel::error:   return "Error";
    }
    return "Unknown";
}


Log::Log(size_t capacity) : max_entries(std::max<size_t>(capacity, 1)) {
}


auto Log::write(LogLevel level, std::string message) -> void {
    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time);

    {
        const auto lock = std::scoped_lock{mutex};

        if (buffer.size() >= max_entries) {
            buffer.pop_front();
        }

        buffer.push_back(LogEntry{
            .sequence = next_sequence++,
            .seconds  = elapsed.count(),
            .level    = level,
            .message  = std::move(message),
        });
    }

    change_count.fetch_add(1, std::memory_order_release);
}


auto Log::clear() -> void {
    {
        const auto lock = std::scoped_lock{mutex};
        buffer.clear();
    }

    change_count.fetch_add(1, std::memory_order_release);
}


auto Log::entries() const -> std::vector<LogEntry> {
    const auto lock = std::scoped_lock{mutex};
    return {buffer.begin(), buffer.end()};
}


auto Log::size() const -> size_t {
    const auto lock = std::scoped_lock{mutex};
    return buffer.size();
}


auto app_log() -> Log& {
    static auto log = Log{};
    return log;
}

} // namespace c8
