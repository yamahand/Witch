#pragma once
#include "WitchEngine/Core/Log/ILogFilter.h"
#include "WitchEngine/Core/Log/ILogFormatter.h"
#include "WitchEngine/Core/Log/ILogSink.h"
#include <memory>

namespace witch::log {

/// デバッガ出力（Windows では OutputDebugStringA）への Immediate Sink。
/// クラッシュ直前のログも確実にデバッガへ届くよう、遅延させず即時出力する。
/// 非 Windows では no-op（1 関数分の差なので Platform/ 分割はせず #ifdef で吸収。
/// 従来の Logger.cpp / Time.cpp と同じ方針）。
class DebugOutputSink final : public ILogSink {
public:
    /// formatter 省略時は PlainTextFormatter。filter は null なら全件通す。
    explicit DebugOutputSink(std::unique_ptr<ILogFormatter> formatter = nullptr,
                             std::unique_ptr<ILogFilter> filter = nullptr);

    [[nodiscard]] SinkMode Mode() const override { return SinkMode::Immediate; }
    void Write(const LogView& view) override;

private:
    std::unique_ptr<ILogFormatter> formatter_;
    std::unique_ptr<ILogFilter> filter_;
};

} // namespace witch::log
