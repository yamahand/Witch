#pragma once
#include <cstdint>

namespace witch {

/// 描画レイヤー（int16_t、大きいほど手前）を RHI の sortKey（同一 space 内の
/// 順序キー）に変換する。空間（World/Screen）は SpriteDrawDesc.space で渡し、
/// RHI が space 主・sortKey 副でソートする。
/// bits 8..23: layer（int16_t + 0x8000 バイアスで昇順化）。bits 0..7 は予約。
/// SpriteComponent / TilemapComponent が共用する（エンコードの単一ソース）。
constexpr uint32_t LayerToSortKey(int16_t layer) {
    return static_cast<uint32_t>(static_cast<uint16_t>(layer + 0x8000)) << 8;
}

} // namespace witch
