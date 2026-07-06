#pragma once
#include "WitchEngine/Core/Log/LogLevel.h"
#include <chrono>
#include <cstdint>

namespace witch::log {

/// Logger 内部専用のログ 1 件分。文字列本体は持たず、TextBuffer 内の
/// オフセット＋長さで指す。Sink へはこの型を渡さず、Logger が LogView へ変換する
/// （Sink は LogRecord を知らない、という契約の実体）。
struct LogRecord {
    uint64_t sequence = 0;
    std::chrono::system_clock::time_point timestamp;
    uint64_t frameNumber = 0;
    size_t threadId = 0;
    LogLevel level = LogLevel::Info;
    uint64_t categoryHash = 0;
    uint32_t categoryOffset = 0;
    uint32_t categoryLength = 0;
    uint32_t messageOffset = 0;
    uint32_t messageLength = 0;
    const char* file = "";     ///< source_location の文字列リテラル（静的寿命）を指す
    const char* function = "";
    uint32_t line = 0;
};

} // namespace witch::log
