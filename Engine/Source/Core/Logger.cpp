#include "WitchEngine/Core/Logger.h"
#include <chrono>
#include <cstdio>
#include <format>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace witch::log {

namespace {

std::string Timestamp() {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    auto t = system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    return std::format("{:02}:{:02}:{:02}.{:03}",
                       tm.tm_hour, tm.tm_min, tm.tm_sec, ms.count());
}

void Output(const char* level, std::string_view msg) {
    auto line = std::format("[{}][{}] {}\n", Timestamp(), level, msg);
    std::fputs(line.c_str(), stdout);
#ifdef _WIN32
    OutputDebugStringA(line.c_str());
#endif
}

} // namespace

void Info(std::string_view msg) { Output("INFO", msg); }
void Warn(std::string_view msg) { Output("WARN", msg); }
void Error(std::string_view msg) { Output("ERR ", msg); }

} // namespace witch::log
