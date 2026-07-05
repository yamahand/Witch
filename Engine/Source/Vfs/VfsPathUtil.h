#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace witch::vfs::detail {

// バックスラッシュをスラッシュに統一し、先頭スラッシュ・"." を除去する。
// ".." を含む場合はトラバーサル攻撃の可能性があるため nullopt を返す。
[[nodiscard]] std::optional<std::string> NormalizePath(std::string_view path);

// NormalizePath に加えて小文字化する（ストライプロックのキー用）。
[[nodiscard]] std::optional<std::string> NormalizePathForLock(std::string_view path);

// 拡張子（ドット含む小文字、例: ".png"）を返す。なければ空文字列。
[[nodiscard]] std::string GetExtension(std::string_view path);

}  // namespace witch::vfs::detail
