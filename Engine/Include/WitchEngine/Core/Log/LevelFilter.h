#pragma once
#include "WitchEngine/Core/Log/ILogFilter.h"
#include "WitchEngine/Core/Log/LogView.h"

namespace witch::log {

/// 指定レベル以上のみ通す閾値フィルタ。
class LevelFilter final : public ILogFilter {
public:
    explicit LevelFilter(LogLevel threshold) : threshold_(threshold) {}

    [[nodiscard]] bool Accept(const LogView& view) const override {
        return view.level >= threshold_;
    }

private:
    LogLevel threshold_;
};

} // namespace witch::log
