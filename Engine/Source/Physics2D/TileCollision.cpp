#include "WitchEngine/Physics2D/TileCollision.h"
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

bool IsSolid(const LevelIntGrid& g, int cx, int cy) {
    if (cx < 0 || cy < 0 || cx >= g.width || cy >= g.height) {
        return false;  // グリッド外 = 空
    }
    const size_t index =
        static_cast<size_t>(cy) * static_cast<size_t>(g.width) + static_cast<size_t>(cx);
    if (index >= g.values.size()) {
        return false;  // サイズ不整合データは空扱い（ローダが保証するが防御）
    }
    return ShapeFromValue(g.values[index]) == TileShape::Solid;
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
/// 1 セルを超えてもトンネリングしない）。
float SweepX(const LevelIntGrid& g, const Aabb& box, float dx,
             bool& hitLeft, bool& hitRight) {
    const int gs = g.gridSize;
    if (gs <= 0 || dx == 0.0f) {
        return box.x + dx;
    }
    const int rowMin = FloorCell(box.y, gs);
    const int rowMax = LastCell(box.y + box.h, gs);
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

/// Y 軸の掃引（SweepX の縦横入替。y-down なので dy > 0 = 下 = 接地側）。
float SweepY(const LevelIntGrid& g, const Aabb& box, float dy,
             bool& hitHead, bool& onGround) {
    const int gs = g.gridSize;
    if (gs <= 0 || dy == 0.0f) {
        return box.y + dy;
    }
    const int colMin = FloorCell(box.x, gs);
    const int colMax = LastCell(box.x + box.w, gs);
    if (dy > 0.0f) {
        const float targetBottom = box.y + box.h + dy;
        for (int c = LastCell(box.y + box.h, gs) + 1;
             static_cast<float>(c) * static_cast<float>(gs) < targetBottom; ++c) {
            if (AnySolidInRow(g, c, colMin, colMax)) {
                onGround = true;
                return static_cast<float>(c * gs) - box.h;
            }
        }
        return targetBottom - box.h;
    }
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

} // namespace

TileShape ShapeFromValue(int value) {
    return value == 0 ? TileShape::Empty : TileShape::Solid;
}

MoveResult MoveAabb(const LevelIntGrid& grid, const Aabb& box, float dx, float dy) {
    MoveResult r;
    Aabb cur = box;
    cur.x = SweepX(grid, cur, dx, r.hitLeft, r.hitRight);
    cur.y = SweepY(grid, cur, dy, r.hitHead, r.onGround);
    r.x = cur.x;
    r.y = cur.y;
    return r;
}

const LevelIntGrid* FindCollisionGrid(const LevelData& level) {
    return level.intGrids.empty() ? nullptr : &level.intGrids.front();
}

} // namespace witch::physics2d
