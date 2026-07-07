#pragma once
#include "WitchEngine/Core/Log/ILogFilter.h"
#include "WitchEngine/Core/Log/ILogFormatter.h"
#include "WitchEngine/Core/Log/ILogSink.h"
#include <expected>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace witch::log {

/// ファイルへの Deferred Sink。生成は Create() 経由のみ（開けない場合は例外ではなく
/// std::expected でエラーを返す。エンジンのエラー処理方針に従う）。
/// Write/Flush は Logger のロック下でのみ呼ばれるため自前のロックは持たない。
class FileSink final : public ILogSink {
public:
    /// path を新規作成（上書き）で開く。親ディレクトリが無ければ作る。
    /// formatter 省略時は DetailedTextFormatter（ファイルには詳細な記録を残す）。
    [[nodiscard]] static std::expected<std::unique_ptr<FileSink>, std::string>
    Create(const std::filesystem::path& path,
           std::unique_ptr<ILogFormatter> formatter = nullptr,
           std::unique_ptr<ILogFilter> filter = nullptr);

    [[nodiscard]] SinkMode Mode() const override { return SinkMode::Deferred; }
    void Write(const LogView& view) override;
    void Flush() override;

private:
    FileSink(std::ofstream file, std::unique_ptr<ILogFormatter> formatter,
             std::unique_ptr<ILogFilter> filter);

    std::ofstream file_;
    std::unique_ptr<ILogFormatter> formatter_;
    std::unique_ptr<ILogFilter> filter_;
    std::string pending_;
};

} // namespace witch::log
