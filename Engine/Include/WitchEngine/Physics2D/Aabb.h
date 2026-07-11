#pragma once

namespace witch {

/// 軸平行境界箱（Axis-Aligned Bounding Box）。左上 + サイズ、y-down
/// （レベル px 座標系。上 = -Y、下 = +Y。LDtk / スクリーン座標と同じ系）。
struct Aabb {
    float x = 0.0f;  ///< 左端
    float y = 0.0f;  ///< 上端
    float w = 0.0f;  ///< 幅
    float h = 0.0f;  ///< 高さ

    float Right() const { return x + w; }
    float Bottom() const { return y + h; }

    /// 重なり判定。辺がちょうど接している（touching）だけでは重ならない扱い。
    bool Overlaps(const Aabb& o) const {
        return x < o.x + o.w && o.x < x + w &&
               y < o.y + o.h && o.y < y + h;
    }
};

} // namespace witch
