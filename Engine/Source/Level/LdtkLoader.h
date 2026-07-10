#pragma once
#include "WitchEngine/Level/LevelData.h"
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>

namespace witch::ldtk {

/// .ldtk（LDtk の JSON プロジェクト）をパースし、先頭レベル（levels[0]）を
/// フォーマット中立の LevelData に正規化する。複数レベルの選択は必要になってから
/// 引数を足す。externalLevels（レベル別ファイル保存）は未対応でエラーを返す。
/// nlohmann-json の例外は実装内で std::expected に翻訳する
/// （エンジン内部へ例外を伝播させない。D3D12 / stb と同じ境界隔離）。
/// @param bytes ファイル全体のバイト列
/// @param sourceName エラー・警告ログに出す識別名（VFS パス）。タイルセット
///                   relPath はこのパスのディレクトリ基準で VFS パスへ解決する。
std::expected<LevelData, std::string> ParseLdtk(std::span<const uint8_t> bytes,
                                                std::string_view sourceName);

} // namespace witch::ldtk
