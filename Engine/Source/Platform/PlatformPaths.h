#pragma once
#include <filesystem>

namespace witch::platform {

/// 実行中の実行ファイルが置かれているディレクトリを返す。
/// 取得失敗時はカレントディレクトリにフォールバックする。
std::filesystem::path GetExecutableDir();

} // namespace witch::platform
