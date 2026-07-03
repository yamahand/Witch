#pragma once
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <format>
#include <string_view>

namespace witch {

// スタック上の固定バッファに書き込む一時文字列。ヒープ確保なし。
// 主な用途はデバッグ UI / ログ表示など、毎フレーム作っては捨てる短い文字列。
// バッファ溢れは開発中の不整合として assert で検出する（黙って切り詰めない）。
template<std::size_t N>
class FixedString {
public:
    FixedString() = default;

    void Clear() {
        size_ = 0;
        buffer_[0] = '\0';
    }

    /// 文字列を追加する。バッファが溢れる場合は assert で検出する。
    void Append(std::string_view text) {
        assert(size_ + text.size() < N);
        std::size_t copyLen = std::min(text.size(), N - 1 - size_);
        std::copy_n(text.data(), copyLen, buffer_ + size_);
        size_ += copyLen;
        buffer_[size_] = '\0';
    }

    /// 他の FixedString を追加する。バッファが溢れる場合は assert で検出する。
    template<std::size_t M>
    void Append(const FixedString<M>& other) {
        assert(size_ + other.size_ < N);
        std::size_t copyLen = std::min(other.size_, N - 1 - size_);
        std::copy_n(other.buffer_, copyLen, buffer_ + size_);
        size_ += copyLen;
        buffer_[size_] = '\0';
    }

    /// フォーマット付きで文字列を設定する。バッファが溢れる場合は assert で検出する。
    template<typename... Args>
    void Format(std::format_string<Args...> fmt, Args&&... args) {
        auto result = std::format_to_n(buffer_, N - 1, fmt, std::forward<Args>(args)...);
        assert(result.size <= static_cast<std::ptrdiff_t>(N - 1));
        size_ = static_cast<std::size_t>(result.size);
        buffer_[size_] = '\0';
    }

    /// フォーマット付きで文字列を追加する。バッファが溢れる場合は assert で検出する。
    template<typename... Args>
    void AppendFormat(std::format_string<Args...> fmt, Args&&... args) {
        auto result = std::format_to_n(buffer_ + size_, N - 1 - size_, fmt, std::forward<Args>(args)...);
        assert(result.size <= static_cast<std::ptrdiff_t>(N - 1 - size_));
        size_ += static_cast<std::size_t>(result.size);
        buffer_[size_] = '\0';
    }

    [[nodiscard]] const char* c_str() const { return buffer_; }
    [[nodiscard]] std::string_view View() const { return std::string_view(buffer_, size_); }
    [[nodiscard]] std::size_t Size() const { return size_; }
    [[nodiscard]] static constexpr std::size_t Capacity() { return N - 1; }

private:
    char buffer_[N] = {};
    std::size_t size_ = 0;
};

// 32, 64, 128, 256 バイトの固定文字列型を定義
using FixedString32 = FixedString<32>;
using FixedString64 = FixedString<64>;
using FixedString128 = FixedString<128>;
using FixedString256 = FixedString<256>;

} // namespace witch
