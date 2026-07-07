#include "WitchEngine/Core/Log/DebugOutputSink.h"
#include "WitchEngine/Core/Log/PlainTextFormatter.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace witch::log {

DebugOutputSink::DebugOutputSink(std::unique_ptr<ILogFormatter> formatter,
                                 std::unique_ptr<ILogFilter> filter)
    : formatter_(formatter ? std::move(formatter) : std::make_unique<PlainTextFormatter>()),
      filter_(std::move(filter)) {}

void DebugOutputSink::Write(const LogView& view) {
    if (filter_ && !filter_->Accept(view)) {
        return;
    }
#ifdef _WIN32
    std::string line = formatter_->Format(view);
    line += '\n';
    OutputDebugStringA(line.c_str());
#else
    (void)view;
#endif
}

} // namespace witch::log
