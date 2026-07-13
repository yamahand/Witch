#include "WitchEngine/Physics2D/TileCollision.h"
#include <algorithm>
#include <cmath>

namespace witch::physics2d {

namespace {

/// edge が属するセル番号（床関数。整数除算の切り捨てだと負座標で 1 ずれるため）。
int FloorCell(float edge, int gridSize) {
    return static_cast<int>(std::floor(edge / static_cast<float>(gridSize)));
}

/// 右端 / 下端 edge が最後に占めるセル番号。edge がセル境界ちょうどの場合は
/// 境界の手前のセルを返す（境界に接しているだけでは次のセルを占めない。
/// 床の上に静止した箱の下端がちょうど床タイルの上辺に一致するケースで、
/// 床タイルを「占有中」と誤判定して横移動が壁扱いになるのを防ぐ）。
int LastCell(float edge, int gridSize) {
    const float cells = edge / static_cast<float>(gridSize);
    int idx = static_cast<int>(std::floor(cells));
    if (static_cast<float>(idx) == cells) {
        --idx;
    }
    return idx;
}

/// セルの衝突形状（グリッド外・サイズ不整合データは空扱い）。
TileShape ShapeAt(const LevelIntGrid& g, int cx, int cy) {
    if (cx < 0 || cy < 0 || cx >= g.width || cy >= g.height) {
        return TileShape::Empty;
    }
    const size_t index =
        static_cast<size_t>(cy) * static_cast<size_t>(g.width) + static_cast<size_t>(cx);
    if (index >= g.values.size()) {
        return TileShape::Empty;
    }
    return ShapeFromValue(g.values[index]);
}

bool IsSlope(TileShape s) {
    return s == TileShape::SlopeUpRight || s == TileShape::SlopeUpLeft;
}

/// 坂セル (cx, cy) 内の x = px における表面の y 座標（y-down なので小さいほど高い）。
/// px はセル範囲へクランプされる。
float SlopeSurfaceY(TileShape shape, int cx, int cy, int gridSize, float px) {
    const float gs = static_cast<float>(gridSize);
    const float left = static_cast<float>(cx) * gs;
    const float t = std::clamp((px - left) / gs, 0.0f, 1.0f);
    const float top = static_cast<float>(cy) * gs;
    // '/' は左端 = 下辺 → 右端 = 上辺、'\' はその逆。
    return shape == TileShape::SlopeUpRight ? top + gs - t * gs : top + t * gs;
}

bool IsSolid(const LevelIntGrid& g, int cx, int cy) {
    return ShapeAt(g, cx, cy) == TileShape::Solid;
}

bool AnySolidInColumn(const LevelIntGrid& g, int cx, int rowMin, int rowMax) {
    for (int cy = rowMin; cy <= rowMax; ++cy) {
        if (IsSolid(g, cx, cy)) {
            return true;
        }
    }
    return false;
}

bool AnySolidInRow(const LevelIntGrid& g, int cy, int colMin, int colMax) {
    for (int cx = colMin; cx <= colMax; ++cx) {
        if (IsSolid(g, cx, cy)) {
            return true;
        }
    }
    return false;
}

/// X 軸の掃引。box の縦方向（y / h）は固定のまま x を dx 動かし、進入する
/// セル列を進行方向順に検査して最初のソリッドで停止する（1 ステップの移動量が
/// 1 セルを超えてもトンネリングしない）。坂セルはソリッド扱いしない。
float SweepX(const LevelIntGrid& g, const Aabb& box, float dx,
             bool& hitLeft, bool& hitRight) {
    const int gs = g.gridSize;
    if (gs <= 0 || dx == 0.0f) {
        return box.x + dx;
    }
    const int rowMin = FloorCell(box.y, gs);
    int rowMax = LastCell(box.y + box.h, gs);
    // 足元中心セルが坂なら足元の行を側面判定から除外する（登坂中に頂上の平地
    // セルの角で引っかからないため。TileCollision.h の坂の契約参照）。
    // ただし AABB が縦 1 行にしか跨がないとき（rowMin == rowMax）は除外しない:
    // 除外すると rowMax < rowMin になり、その X 掃引の壁判定が全カラムで
    // 無効化され、隣接する通常の壁もすり抜けてしまうため（足元行しか無いので
    // 除外すべき「頂上の平地セルの角」も存在せず、除外の必要がない）。
    const int footCol = FloorCell(box.x + box.w * 0.5f, gs);
    if (rowMax > rowMin && IsSlope(ShapeAt(g, footCol, rowMax))) {
        --rowMax;
    }
    if (dx > 0.0f) {
        const float targetRight = box.x + box.w + dx;
        // 現在の右端以右で最初のセル列（右端が境界ちょうどなら右隣の列）から昇順に検査。
        // 既に食い込んでいる列は対象外（後退テレポートさせない）。
        for (int c = LastCell(box.x + box.w, gs) + 1;
             static_cast<float>(c) * static_cast<float>(gs) < targetRight; ++c) {
            if (AnySolidInColumn(g, c, rowMin, rowMax)) {
                hitRight = true;
                return static_cast<float>(c * gs) - box.w;
            }
        }
        return targetRight - box.w;
    }
    const float targetLeft = box.x + dx;
    // 現在の左端以左で最初のセル列（右境界 <= 左端）から降順に検査。
    for (int c = FloorCell(box.x, gs) - 1;
         static_cast<float>(c + 1) * static_cast<float>(gs) > targetLeft; --c) {
        if (AnySolidInColumn(g, c, rowMin, rowMax)) {
            hitLeft = true;
            return static_cast<float>((c + 1) * gs);
        }
    }
    return targetLeft;
}

/// Y 軸の掃引（y-down なので dy > 0 = 下 = 接地側）。X 解決後の box を受け取る。
/// 下向きはソリッド掃引 → 坂の足元補正（中心点方式） → アンスティック →
/// 接地スナップ（snapToGround）の順で解決する。
float SweepY(const LevelIntGrid& g, const Aabb& box, float dy, bool snapToGround,
             bool& hitHead, bool& onGround) {
    const int gs = g.gridSize;
    if (gs <= 0) {
        return box.y + dy;
    }

    if (dy < 0.0f) {
        // 上向き: ソリッドの天井掃引のみ（天井坂は未対応）。
        const int colMin = FloorCell(box.x, gs);
        const int colMax = LastCell(box.x + box.w, gs);
        const float targetTop = box.y + dy;
        for (int c = FloorCell(box.y, gs) - 1;
             static_cast<float>(c + 1) * static_cast<float>(gs) > targetTop; --c) {
            if (AnySolidInRow(g, c, colMin, colMax)) {
                hitHead = true;
                return static_cast<float>((c + 1) * gs);
            }
        }
        return targetTop;
    }

    // ── 下向き（dy == 0 でも坂の押し上げ補正・アンスティックは行う） ──
    const int colMin = FloorCell(box.x, gs);
    const int colMax = LastCell(box.x + box.w, gs);
    const float px = box.x + box.w * 0.5f;  // 足元中心点の x
    const int footCol = FloorCell(px, gs);
    const float oldBottom = box.y + box.h;
    float bottom = oldBottom + dy;

    // 1) ソリッド掃引（全幅）。
    if (dy > 0.0f) {
        for (int r = LastCell(oldBottom, gs) + 1;
             static_cast<float>(r) * static_cast<float>(gs) < bottom; ++r) {
            if (AnySolidInRow(g, r, colMin, colMax)) {
                bottom = static_cast<float>(r * gs);
                onGround = true;
                break;
            }
        }
    }

    // 2) 坂の足元補正（中心点方式）: 移動範囲内の最初の坂セルの表面より下へは
    //    沈まない。登坂（X 移動で表面より下に入った）の押し上げもここで起きる。
    //    ソリッドが先に見つかったらそれより下の坂は見ない（ソリッド越しに
    //    スナップしない）。開始行は LastCell（境界ちょうど = 坂セルの下端に
    //    立っているとき、その坂セル自体を検査対象に含めるため）。
    for (int r = LastCell(oldBottom, gs); r <= FloorCell(bottom, gs); ++r) {
        const TileShape s = ShapeAt(g, footCol, r);
        if (IsSlope(s)) {
            const float surface = SlopeSurfaceY(s, footCol, r, gs, px);
            if (bottom > surface) {
                bottom = surface;
                onGround = true;
            }
            break;
        }
        if (s == TileShape::Solid) {
            break;
        }
    }

    // 3) 足元アンスティック: 中心点がソリッドセル内へ食い込んでいたら上辺へ
    //    押し出して接地扱い。坂→平地の乗り移り（X 掃引の足元行除外 + 大きめの
    //    重力ステップ）が残す食い込みを解消する。中心点がセル内にある限り深さは
    //    必ず 1 セル未満で、壁は X 掃引で止まるため中心点が横から深く入ることは
    //    ない（1 セル以上の不正な食い込み = ワープ等はここでは触らない）。
    {
        const int fr = LastCell(bottom, gs);
        if (ShapeAt(g, footCol, fr) == TileShape::Solid) {
            const float top = static_cast<float>(fr * gs);
            const float depth = bottom - top;
            if (depth > 0.0f && depth < static_cast<float>(gs)) {
                bottom = top;
                onGround = true;
            }
        }
    }

    // 4) 接地スナップ: 非接地に終わったが直下 1 セル以内に地面があれば吸着する
    //    （下り坂・小さな段差降りの浮き防止。呼び出し側が前ステップ接地中のみ渡す）。
    if (!onGround && snapToGround) {
        const float maxSnap = bottom + static_cast<float>(gs);
        float candidate = maxSnap + 1.0f;  // 番兵（範囲外）
        for (int r = LastCell(bottom, gs) + 1;
             static_cast<float>(r) * static_cast<float>(gs) <= maxSnap; ++r) {
            if (AnySolidInRow(g, r, colMin, colMax)) {
                candidate = static_cast<float>(r * gs);
                break;
            }
        }
        // 坂候補の開始行も LastCell（下端が坂セルの上辺境界ちょうどのケースを拾う）。
        for (int r = LastCell(bottom, gs);
             static_cast<float>(r) * static_cast<float>(gs) <= maxSnap; ++r) {
            const TileShape s = ShapeAt(g, footCol, r);
            if (IsSlope(s)) {
                const float surface = SlopeSurfaceY(s, footCol, r, gs, px);
                if (surface >= bottom && surface <= maxSnap) {
                    candidate = std::min(candidate, surface);
                }
                break;
            }
            if (s == TileShape::Solid) {
                break;
            }
        }
        if (candidate <= maxSnap) {
            bottom = candidate;
            onGround = true;
        }
    }

    return bottom - box.h;
}

} // namespace

TileShape ShapeFromValue(int value) {
    switch (value) {
        case 0:  return TileShape::Empty;
        case 2:  return TileShape::SlopeUpRight;
        case 3:  return TileShape::SlopeUpLeft;
        default: return TileShape::Solid;
    }
}

MoveResult MoveAabb(const LevelIntGrid& grid, const Aabb& box, float dx, float dy,
                    bool snapToGround) {
    MoveResult r;
    Aabb cur = box;
    cur.x = SweepX(grid, cur, dx, r.hitLeft, r.hitRight);
    cur.y = SweepY(grid, cur, dy, snapToGround, r.hitHead, r.onGround);
    r.x = cur.x;
    r.y = cur.y;
    return r;
}

const LevelIntGrid* FindCollisionGrid(const LevelData& level) {
    return level.intGrids.empty() ? nullptr : &level.intGrids.front();
}

} // namespace witch::physics2d
