#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace witch::log {

/// ログ文字列を追記していく巨大フラットバッファ。Logger がダブルバッファで 2 面持ち、
/// Flush 時にスワップする。スレッド保護は呼び出し側（Logger のロック）が行う前提で、
/// この型自体はロックしない（将来この Append をアトミックな bump allocator に置き換えれば
/// ロックフリー化の第一候補になる）。
class TextBuffer {
public:
    explicit TextBuffer(size_t capacity) : buffer_(capacity) {}

    /// 追記して書き込み開始オフセットを返す。容量不足なら nullopt
    /// （呼び出し側が Flush→スワップして空きを作ってから再試行する）。
    [[nodiscard]] std::optional<uint32_t> Append(std::string_view text);

    [[nodiscard]] std::string_view View(uint32_t offset, uint32_t length) const {
        return std::string_view(buffer_.data() + offset, length);
    }

    void Reset() { size_ = 0; }
    [[nodiscard]] size_t Size() const { return size_; }
    [[nodiscard]] size_t Capacity() const { return buffer_.size(); }

private:
    std::vector<char> buffer_;
    size_t size_ = 0;
};

} // namespace witch::log
