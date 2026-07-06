#include "WitchEngine/Core/Log/FileSink.h"
#include "WitchEngine/Core/Log/DetailedTextFormatter.h"
#include <format>

namespace witch::log {

std::expected<std::unique_ptr<FileSink>, std::string>
FileSink::Create(const std::filesystem::path& path,
                 std::unique_ptr<ILogFormatter> formatter,
                 std::unique_ptr<ILogFilter> filter) {
    // filesystem の例外を投げるオーバーロードは使わない（error_code 版で受ける）。
    std::error_code ec;
    if (const auto parent = path.parent_path(); !parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            return std::unexpected(std::format("Failed to create log directory {}: {}",
                                               parent.string(), ec.message()));
        }
    }

    std::ofstream file(path, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
        return std::unexpected(std::format("Failed to open log file: {}", path.string()));
    }

    return std::unique_ptr<FileSink>(new FileSink(std::move(file),
                                                  std::move(formatter), std::move(filter)));
}

FileSink::FileSink(std::ofstream file, std::unique_ptr<ILogFormatter> formatter,
                   std::unique_ptr<ILogFilter> filter)
    : file_(std::move(file)),
      formatter_(formatter ? std::move(formatter) : std::make_unique<DetailedTextFormatter>()),
      filter_(std::move(filter)) {}

void FileSink::Write(const LogView& view) {
    if (filter_ && !filter_->Accept(view)) {
        return;
    }
    pending_ += formatter_->Format(view);
    pending_ += '\n';
}

void FileSink::Flush() {
    if (pending_.empty()) {
        return;
    }
    file_.write(pending_.data(), static_cast<std::streamsize>(pending_.size()));
    file_.flush(); // クラッシュ時のロスト対策として OS レベルでも都度フラッシュする
    pending_.clear();
}

} // namespace witch::log
