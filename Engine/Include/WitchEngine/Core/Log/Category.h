#pragma once
#include <cstdint>
#include <string_view>

namespace witch::log {

/// FNV-1a 64bit。カテゴリの比較をハッシュで済ませ、フィルタ時の文字列比較を無くす。
[[nodiscard]] constexpr uint64_t HashCategory(std::string_view text) noexcept {
    uint64_t hash = 14695981039346656037ull;
    for (char c : text) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 1099511628211ull;
    }
    return hash;
}

/// ログのカテゴリ。名前と事前計算済みハッシュの組。
/// リテラルから constexpr で作れる: constexpr log::Category kPhysics{"Physics"};
/// 既定構築（hash == 0）は「カテゴリ無し」を表す番兵として扱う。
struct Category {
    std::string_view name;
    uint64_t hash = 0;

    constexpr Category() = default;
    explicit constexpr Category(std::string_view n) noexcept
        : name(n), hash(HashCategory(n)) {}
};

} // namespace witch::log
