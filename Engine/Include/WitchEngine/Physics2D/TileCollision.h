#pragma once
#include "WitchEngine/Level/LevelData.h"
#include "WitchEngine/Physics2D/Aabb.h"
#include <cstdint>

namespace witch::physics2d {

/// IntGrid のセル値が表す衝突形状。値の割当はレベル側の定義に従う:
/// 0 = 空、2 = 右上がり床坂 '/'（右へ行くほど高い）、3 = 左上がり床坂 '\'、
/// それ以外 = ソリッド。天井坂は消費者が現れてから追加する。
enum class TileShape : uint8_t {
    Empty,
    Solid,
    SlopeUpRight,  ///< '/' 右へ行くほど高い床坂（セル左端 = 下辺、右端 = 上辺）
    SlopeUpLeft,   ///< '\' 左へ行くほど高い床坂（セル左端 = 上辺、右端 = 下辺）
};

/// セル値 → 衝突形状。IntGrid 値の解釈をここ 1 箇所に集約する。
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
/// - onGround は「このステップで +Y 移動が遮られた / 坂・地面に吸着した」の意味。
///   静止中も重力が毎ステップ +Y の移動を与える使い方（CollisionComponent）を
///   前提とする。
///
/// ## 坂（45° 床坂、洞窟物語の中心点方式）
/// - 坂の接地判定は AABB 下辺の**中心点**で行う（幅全体ではない）。
/// - X 掃引は坂セルをソリッド扱いしない。さらに足元中心セルが坂で、かつ AABB が
///   縦 2 行以上に跨がるときは **足元の行を側面判定から除外**する（坂を登り切って
///   頂上の平地セルへ乗り移る際、平地セルの角で引っかからないため）。副作用として、
///   足元が坂セル内にある間は「足元行にしか無い高さ 1 セルの障害物」を側面で
///   すり抜けうる（通常のマップ構成では起きない。壁は複数セル高 or 坂に隣接しない
///   前提）。AABB が縦 1 行にしか跨がらないときは除外しない（除外すると検査行が
///   空になり隣接壁もすり抜けるため。1 行しかないなら乗り移り対象の「上の行」も
///   無く除外の必要がない）。
/// - 中心点がソリッドセル内へ食い込んだ場合（深さは常に 1 セル未満）は上へ
///   押し出して接地扱いにする（坂→平地の乗り移りで残る食い込みの解消）。
/// @param snapToGround 真のとき、非接地に終わる下向き移動で直下 1 セル以内に
///   地面（ソリッド上辺 or 坂表面）があればそこへ吸着して onGround にする。
///   下り坂で毎ステップ浮いてしまうのを防ぐ（前ステップ接地中かつ上向き速度が
///   無いときに渡す。1 セル以内の段差降りも吸着する = 階段的な下りが滑らかになる）。
MoveResult MoveAabb(const LevelIntGrid& grid, const Aabb& box, float dx, float dy,
                    bool snapToGround = false);

/// 衝突判定に使う IntGrid の取得規約: intGrids の先頭を返す（無ければ nullptr）。
/// レイヤー名規約は設けない（サンプルレベルの IntGrid は 1 枚のみ。複数必要に
/// なったら identifier 引数を足す）。
const LevelIntGrid* FindCollisionGrid(const LevelData& level);

} // namespace witch::physics2d
