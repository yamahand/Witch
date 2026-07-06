#include "Core/Log/TextBuffer.h"
#include <cstring>

namespace witch::log {

std::optional<uint32_t> TextBuffer::Append(std::string_view text) {
    if (size_ + text.size() > buffer_.size()) {
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
