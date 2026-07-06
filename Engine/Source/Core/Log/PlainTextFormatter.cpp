#include "WitchEngine/Core/Log/PlainTextFormatter.h"
#include "WitchEngine/Core/Log/LogView.h"
#include "Core/Log/TimestampFormat.h"
#include <format>

namespace witch::log {

std::string PlainTextFormatter::Format(const LogView& view) const {
    if (view.category.empty()) {
        return std::format("[{}][{:<5}] {}",
                           FormatTimestamp(view.timestamp), ToString(view.level), view.message);
    }
    return std::format("[{}][{:<5}][{}] {}",
                       FormatTimestamp(view.timestamp), ToString(view.level),
                       view.category, view.message);
}

} // namespace witch::log
