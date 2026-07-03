#pragma once
#include "WitchEngine/Rhi/RhiTypes.h"

namespace witch {

/// ロード済みテクスチャのハンドルとピクセルサイズ。
/// ピクセル指定のソース矩形（スプライトシートのコマ切り出し）を UV に換算するには
/// テクスチャの実サイズが必要なため、ResourceManager::LoadTexture はハンドル単体で
/// なくこの構造体を返す。
struct TextureInfo {
    rhi::TextureHandle handle;
    int width  = 0;
    int height = 0;

    bool IsValid() const { return handle.IsValid(); }
};

} // namespace witch
