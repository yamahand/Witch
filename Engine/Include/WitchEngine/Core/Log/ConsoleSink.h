#pragma once
#include "WitchEngine/Core/Log/ILogFilter.h"
#include "WitchEngine/Core/Log/ILogFormatter.h"
#include "WitchEngine/Core/Log/ILogSink.h"
#include <memory>
#include <string>

namespace witch::log {

/// stdout への Deferred Sink。Write() で整形済み行を溜め、Flush() でまとめて書き出す
/// （ログ 1 件毎の I/O システムコールを避け、フレーム単位にバッチする）。
/// Write/Flush は Logger のロック下でのみ呼ばれるため自前のロックは持たない。
class ConsoleSink final : public ILogSink {
public:
    /// formatter 省略時は PlainTextFormatter。filter は null なら全件通す。
    explicit ConsoleSink(std::unique_ptr<ILogFormatter> formatter = nullptr,
                         std::unique_ptr<ILogFilter> filter = nullptr);

    [[nodiscard]] SinkMode Mode() const override { return SinkMode::Deferred; }
    void Write(const LogView& view) override;
    void Flush() override;

private:
    std::unique_ptr<ILogFormatter> formatter_;
    std::unique_ptr<ILogFilter> filter_;
    std::string pending_;
};

} // namespace witch::log
