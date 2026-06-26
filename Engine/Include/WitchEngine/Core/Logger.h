#pragma once
#include <string_view>
#include <format>

namespace witch::log {

/// ログレベル別の出力関数。`std::format` 書式文字列のオーバーロードも持つ。
void Info(std::string_view msg);
void Warn(std::string_view msg);
void Error(std::string_view msg);

template<typename... Args>
void Info(std::format_string<Args...> fmt, Args&&... args) {
    Info(std::format(fmt, std::forward<Args>(args)...));
}

template<typename... Args>
void Warn(std::format_string<Args...> fmt, Args&&... args) {
    Warn(std::format(fmt, std::forward<Args>(args)...));
}

template<typename... Args>
void Error(std::format_string<Args...> fmt, Args&&... args) {
    Error(std::format(fmt, std::forward<Args>(args)...));
}

} // namespace witch::log
