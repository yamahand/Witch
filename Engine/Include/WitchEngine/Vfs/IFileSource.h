#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace witch::vfs {

// ファイルソースの抽象。DiskSource が実装する。
// 受け取るパスは正規化済み（スラッシュ区切り、先頭スラッシュなし、".." なし）前提。
class IFileSource {
public:
    virtual ~IFileSource() = default;

    [[nodiscard]] virtual bool Exists(std::string_view normalized_path) const = 0;
    [[nodiscard]] virtual bool ReadFile(std::string_view normalized_path,
                                        std::vector<uint8_t>& out) const = 0;
    // normalized_dir 直下のファイルのみ返す（サブディレクトリを再帰しない）。
    [[nodiscard]] virtual std::vector<std::string> ListFiles(
                                        std::string_view normalized_dir) const = 0;
};

}  // namespace witch::vfs
