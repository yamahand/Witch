#pragma once
#include "WitchEngine/Core/Log/LogLevel.h"
#include <chrono>
#include <cstdint>
#include <string_view>

namespace witch::log {

/// Sink へ渡す読み取り専用のログ 1 件分。内部表現（LogRecord）を隠蔽するビュー。
/// category / message は Logger 内部の TextBuffer を指すため、Write() 呼び出し中のみ有効。
/// 呼び出しを越えて保持したい Sink は自前のストレージへコピーすること（ViewerSink 参照）。
struct LogView {
    uint64_t sequence = 0;
    std::chrono::system_clock::time_point timestamp;
    uint64_t frameNumber = 0;
    size_t threadId = 0;
    LogLevel level = LogLevel::Info;
    uint64_t categoryHash = 0;
    std::string_view category;
    std::string_view message;
    const char* file = "";
    const char* function = "";
    uint32_t line = 0;
};

} // namespace witch::log
