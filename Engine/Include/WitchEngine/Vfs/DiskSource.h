#pragma once

#include "WitchEngine/Vfs/IFileSource.h"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>

namespace witch::vfs {

// ディスク上の実ディレクトリをルートとするファイルソース。
// SafeResolve でパストラバーサル・シンボリックリンク脱出を防ぐ。
// WriteFile は新規ファイルの場合 .tmp に書いてから rename する（アトミック）。
// 既存ファイルの上書きは target->.old, tmp->target の 2 段階 rename になるため、
// 途中でクラッシュ／電源断が起きると target が一時的に失われた状態になり得る
// （.old と .tmp は残るため手動復旧は可能。エラー時はロールバックを試みる）。
class DiskSource : public IFileSource {
public:
    explicit DiskSource(std::filesystem::path rootPath);

    bool Exists(std::string_view normalizedPath) const override;
    bool ReadFile(std::string_view normalizedPath,
                  std::vector<uint8_t>& out) const override;
    std::vector<std::string> ListFiles(std::string_view normalizedDir) const override;

    std::expected<void, std::string> WriteFile(std::string_view normalizedPath, const void* data, size_t size);

private:
    // 正規化済みパスを root_ 配下の絶対パスに変換する。
    // ルート外へ脱出する場合は nullopt を返す（安全チェック済み）。
    [[nodiscard]] std::optional<std::filesystem::path> SafeResolve(
                      std::string_view normalizedPath) const;

    std::filesystem::path root_;
};

}  // namespace witch::vfs
