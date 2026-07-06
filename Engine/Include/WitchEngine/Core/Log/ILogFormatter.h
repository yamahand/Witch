#pragma once
#include <string>

namespace witch::log {

struct LogView;

/// LogView 1 件を出力用の 1 行へ整形する。改行は含めない（付加は Sink の責務）。
/// Sink 毎に差し替え可能（コンストラクタ注入）。
class ILogFormatter {
public:
    virtual ~ILogFormatter() = default;
    [[nodiscard]] virtual std::string Format(const LogView& view) const = 0;
};

} // namespace witch::log
