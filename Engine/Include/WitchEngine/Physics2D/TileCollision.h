#pragma once
#include "WitchEngine/Level/LevelData.h"
#include "WitchEngine/Physics2D/Aabb.h"
#include <cstdint>

namespace witch::physics2d {

/// IntGrid のセル値が表す衝突形状。値の割当はレベル側の定義に従う
/// （現状: 0 = 空、それ以外 = ソリッド。45° 坂は次段で値 2/3 として追加予定）。
enum class TileShape : uint8_t {
    Empty,
    Solid,
};

/// セル値 → 衝突形状。IntGrid 値の解釈をここ 1 箇所に集約する
/// （坂対応時はこの関数と MoveAabb の各パスだけが変わる）。
TileShape ShapeFromValue(int value);

/// MoveAabb の結果。押し戻し後の位置と、各方向で遮られたかのフラグ。
struct MoveResult {
    float x = 0.0f;          ///< 解決後の AABB 左上 X
    float y = 0.0f;          ///< 解決後の AABB 左上 Y
    bool hitLeft = false;    ///< -X 移動が遮られた
    bool hitRight = false;   ///< +X 移動が遮られた
    bool hitHead = false;    ///< -Y（上）移動が遮られた
    bool onGround = false;   ///< +Y（下）移動が遮られた = 接地
};

/// AABB をタイルグリッドに対して (dx, dy) 移動し、軸ごとに押し戻す
/// （X 掃引 → Y 掃引の順。純関数・Scene 非依存）。
/// - 進行方向のセル境界を順に検査するため、移動量が 1 セルを超えても
///   トンネリングしない。
/// - グリッド外のセルは空扱い（レベル外への落下はレベル側で外周を壁にして防ぐ）。
/// - onGround は「このステップで +Y 移動が遮られた」の意味。静止中も重力が
///   毎ステップ +Y の移動を与える使い方（CollisionComponent）を前提とする。
MoveResult MoveAabb(const LevelIntGrid& grid, const Aabb& box, float dx, float dy);

/// 衝突判定に使う IntGrid の取得規約: intGrids の先頭を返す（無ければ nullptr）。
/// レイヤー名規約は設けない（サンプルレベルの IntGrid は 1 枚のみ。複数必要に
/// なったら identifier 引数を足す）。
const LevelIntGrid* FindCollisionGrid(const LevelData& level);

} // namespace witch::physics2d
