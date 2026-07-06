#include "WitchEngine/Core/Log/DetailedTextFormatter.h"
#include "WitchEngine/Core/Log/LogView.h"
#include "Core/Log/TimestampFormat.h"
#include <format>

namespace witch::log {

std::string DetailedTextFormatter::Format(const LogView& view) const {
    if (view.category.empty()) {
        return std::format("[{}][{:<5}][F{}][T{:08x}] {} ({}:{} {})",
                           FormatTimestamp(view.timestamp), ToString(view.level),
                           view.frameNumber, view.threadId,
                           view.message, view.file, view.line, view.function);
    }
    return std::format("[{}][{:<5}][F{}][T{:08x}][{}] {} ({}:{} {})",
                       FormatTimestamp(view.timestamp), ToString(view.level),
                       view.frameNumber, view.threadId, view.category,
                       view.message, view.file, view.line, view.function);
}

} // namespace witch::log
