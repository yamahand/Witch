#pragma once
#include "WitchEngine/Core/Log/ILogFormatter.h"

namespace witch::log {

/// [HH:MM:SS.mmm][LEVEL][Category] message 形式。従来のコンソール出力と互換の簡潔な整形。
class PlainTextFormatter final : public ILogFormatter {
public:
    [[nodiscard]] std::string Format(const LogView& view) const override;
};

} // namespace witch::log
