#pragma once
#include <filesystem>

namespace witch::platform {

/// 実行中の実行ファイルが置かれているディレクトリを返す。
/// 取得失敗時はカレントディレクトリにフォールバックする。カレントディレクトリの取得にも
/// 失敗した場合は空パスを返す（呼び出し側で MountDisk 等が失敗として扱う）。
std::filesystem::path GetExecutableDir();

} // namespace witch::platform
