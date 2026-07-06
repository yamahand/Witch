#pragma once
#include "WitchEngine/Graphics2D/AsepriteSheet.h"
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace witch::aseprite {

/// ParseAse の結果。テクスチャ生成前の CPU 側データ
/// （テクスチャ化は ResourceManager が IRenderer 経由で行う）。
struct ParseResult {
    int frameWidth  = 0;                ///< 1 コマ = キャンバスの px サイズ
    int frameHeight = 0;
    int atlasWidth  = 0;                ///< 全コマを格子に並べたアトラスの px サイズ
    int atlasHeight = 0;
    std::vector<uint8_t> atlasPixels;   ///< RGBA 4 バイト/px、atlasWidth x atlasHeight
    std::vector<AsepriteFrame> frames;  ///< アトラス上の位置と duration（秒）
    std::vector<AsepriteTag> tags;
};

/// .ase / .aseprite バイナリをパースし、可視レイヤーを合成した全フレームを
/// 1 枚の RGBA アトラスに展開する。RGBA / グレースケール / インデックスの
/// 各カラーモードに対応。ブレンドモードは Normal のみ（他は警告して Normal 扱い）。
/// @param bytes ファイル全体のバイト列
/// @param sourceName エラー・警告ログに出す識別名（VFS パス等）
std::expected<ParseResult, std::string> ParseAse(std::span<const uint8_t> bytes,
                                                 std::string_view sourceName);

} // namespace witch::aseprite
