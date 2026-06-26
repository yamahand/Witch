#pragma once
#include <cstdint>

namespace witch::rhi {

/// RGBA 浮動小数点カラー。各値は 0.0〜1.0。
struct Color { float r, g, b, a; };
/// Clear コマンドのパラメータ。
struct ClearDesc { Color color; };

/// テクスチャへの軽量ハンドル。id=0 は無効を示す。
struct TextureHandle {
    uint32_t id = 0;
    bool IsValid() const { return id != 0; }
};

/// スプライト 1 枚の描画パラメータ。UV は正規化テクスチャ座標（0.0〜1.0）。
struct SpriteDrawDesc {
    TextureHandle texture;
    float x = 0, y = 0;
    float width = 0, height = 0;
    float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
};

} // namespace witch::rhi
