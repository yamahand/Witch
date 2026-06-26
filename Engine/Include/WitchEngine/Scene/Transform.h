#pragma once

namespace witch {

/// GameObject が持つ 2D 空間変換。
struct Transform {
    float x = 0.0f;
    float y = 0.0f;
    float rotation = 0.0f;  ///< ラジアン。反時計回り正。
    float scaleX = 1.0f;
    float scaleY = 1.0f;
};

} // namespace witch
