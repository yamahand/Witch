#pragma once
#include <chrono>
#include <ctime>
#include <format>
#include <string>

namespace witch::log {

/// HH:MM:SS.mmm（ローカル時刻）へ整形する。
/// <chrono> のタイムゾーン変換（zoned_time）は tz データベース不在時に例外を投げるため、
/// 例外を投げない localtime_s / localtime_r を使う（エンジンの例外方針に合わせる）。
inline std::string FormatTimestamp(std::chrono::system_clock::time_point tp) {
    using namespace std::chrono;
    auto ms = duration_cast<milliseconds>(tp.time_since_epoch()) % 1000;
    auto t = system_clock::to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return std::format("{:02}:{:02}:{:02}.{:03}",
                       tm.tm_hour, tm.tm_min, tm.tm_sec, ms.count());
}

} // namespace witch::log
