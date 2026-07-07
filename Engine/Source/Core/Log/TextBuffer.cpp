#include "Core/Log/TextBuffer.h"
#include <cstring>

namespace witch::log {

std::optional<uint32_t> TextBuffer::Append(std::string_view text) {
    // size_ + text.size() の加算オーバーフローを避けるため残容量ベースで判定する。
    if (text.size() > buffer_.size() - size_) {
        return std::nullopt;
    }
    const auto offset = static_cast<uint32_t>(size_);
    if (!text.empty()) {
        std::memcpy(buffer_.data() + size_, text.data(), text.size());
        size_ += text.size();
    }
    return offset;
}

} // namespace witch::log
