#pragma once
#include "WitchEngine/Core/Log/ILogFormatter.h"

namespace witch::log {

/// フレーム番号・スレッド ID・呼び出し元（file:line function）まで含む詳細整形。
/// 耐久性のある記録が欲しい FileSink 向け。
class DetailedTextFormatter final : public ILogFormatter {
public:
    [[nodiscard]] std::string Format(const LogView& view) const override;
};

} // namespace witch::log
