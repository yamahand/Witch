#include "WitchEngine/Core/Log/ConsoleSink.h"
#include "WitchEngine/Core/Log/PlainTextFormatter.h"
#include <cstdio>

namespace witch::log {

ConsoleSink::ConsoleSink(std::unique_ptr<ILogFormatter> formatter,
                         std::unique_ptr<ILogFilter> filter)
    : formatter_(formatter ? std::move(formatter) : std::make_unique<PlainTextFormatter>()),
      filter_(std::move(filter)) {}

void ConsoleSink::Write(const LogView& view) {
    if (filter_ && !filter_->Accept(view)) {
        return;
    }
    pending_ += formatter_->Format(view);
    pending_ += '\n';
}

void ConsoleSink::Flush() {
    if (pending_.empty()) {
        return;
    }
    std::fwrite(pending_.data(), 1, pending_.size(), stdout);
    std::fflush(stdout);
    pending_.clear();
}

} // namespace witch::log
