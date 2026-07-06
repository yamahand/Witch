#include "WitchEngine/Core/Log/ViewerSink.h"

namespace witch::log {

ViewerSink::ViewerSink(size_t capacity) : capacity_(capacity) {
    ring_.reserve(capacity_);
}

void ViewerSink::Write(const LogView& view) {
    Entry entry{
        .sequence = view.sequence,
        .timestamp = view.timestamp,
        .frameNumber = view.frameNumber,
        .threadId = view.threadId,
        .level = view.level,
        .categoryHash = view.categoryHash,
        .category = std::string(view.category),
        .message = std::string(view.message),
        .file = view.file,
        .function = view.function,
        .line = view.line,
    };

    std::lock_guard lock(mutex_);
    if (ring_.size() < capacity_) {
        ring_.push_back(std::move(entry));
    } else {
        ring_[head_] = std::move(entry);
        head_ = (head_ + 1) % capacity_;
    }
}

std::vector<ViewerSink::Entry> ViewerSink::Snapshot() const {
    std::lock_guard lock(mutex_);
    if (ring_.size() < capacity_) {
        return ring_;
    }
    // 満杯のリングは head_（最古）から一周した順で並べ直す。
    std::vector<Entry> out;
    out.reserve(ring_.size());
    auto mid = ring_.begin() + static_cast<std::ptrdiff_t>(head_);
    out.insert(out.end(), mid, ring_.end());
    out.insert(out.end(), ring_.begin(), mid);
    return out;
}

} // namespace witch::log
