#pragma once

#include "WitchEngine/Vfs/IFileSource.h"

#include <filesystem>
#include <optional>

namespace witch::vfs {

// ディスク上の実ディレクトリをルートとするファイルソース。
// SafeResolve でパストラバーサル・シンボリックリンク脱出を防ぐ。
// WriteFile は .tmp に書いてから rename するアトミック書き込みを行う。
class DiskSource : public IFileSource {
public:
    explicit DiskSource(std::filesystem::path root_path);

    bool Exists(std::string_view normalized_path) const override;
    bool ReadFile(std::string_view normalized_path,
                  std::vector<uint8_t>& out) const override;
    std::vector<std::string> ListFiles(std::string_view normalized_dir) const override;

    bool WriteFile(std::string_view normalized_path, const void* data, size_t size);

private:
    // 正規化済みパスを root_ 配下の絶対パスに変換する。
    // ルート外へ脱出する場合は nullopt を返す（安全チェック済み）。
    [[nodiscard]] std::optional<std::filesystem::path> SafeResolve(
                      std::string_view normalized_path) const;

    std::filesystem::path root_;
};

}  // namespace witch::vfs
