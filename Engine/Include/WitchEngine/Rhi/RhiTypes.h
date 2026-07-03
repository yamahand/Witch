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
/// 全フィールドにデフォルト値を持たせ、既存呼び出しはそのまま従来と同じ描画になる。
struct SpriteDrawDesc {
    TextureHandle texture;
    float x = 0, y = 0;
    float width = 0, height = 0;
    float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
    /// テクセルに乗算されるカラー（tint + alpha）。既定は白 = 無変調。
    Color color = {1.0f, 1.0f, 1.0f, 1.0f};
    /// 回転角（ラジアン）。画面上で反時計回りが正。0 なら無回転の高速パス。
    float rotation = 0.0f;
    /// 回転中心。矩形内の正規化座標（0,0=左上, 0.5,0.5=中心, 1,1=右下）。
    float pivotX = 0.5f, pivotY = 0.5f;
};

} // namespace witch::rhi
