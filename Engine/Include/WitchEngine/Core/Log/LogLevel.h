#pragma once
#include <cstdint>
#include <string_view>

namespace witch::log {

/// ログの重要度。値が大きいほど重要（閾値フィルタは >= 比較で判定する）。
enum class LogLevel : uint8_t {
    Trace = 0,
    Info,
    Warn,
    Error,
    Fatal,
};

[[nodiscard]] constexpr std::string_view ToString(LogLevel level) {
    switch (level) {
    case LogLevel::Trace: return "TRACE";
    case LogLevel::Info:  return "INFO";
    case LogLevel::Warn:  return "WARN";
    case LogLevel::Error: return "ERROR";
    case LogLevel::Fatal: return "FATAL";
    }
    return "?????";
}

} // namespace witch::log
