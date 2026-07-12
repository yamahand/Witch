#pragma once
#include "WitchEngine/Scene/GameObject.h"

namespace witch {

/// プレイヤーの当たり判定 AABB サイズ px（スプライトより一回り小さい洞窟物語流）。
/// スポーン位置の計算（StageScene）と OnSpawn の CollisionComponent 生成で共用する。
inline constexpr float kPlayerHitboxWidth = 6.0f;
inline constexpr float kPlayerHitboxHeight = 14.0f;

/// プレイヤーの「種別」を表す薄いクラス。振る舞いは Component に分解する:
/// 描画 = SpriteComponent、衝突・移動 = CollisionComponent、
/// 操作（重力・ジャンプ含む）= PlayerControllerComponent。
class PlayerObject : public GameObject {
public:
    /// LoadLevel（ObjectRegistry 経由の nullary 生成）用。
    /// transform は LoadLevel が OnSpawn より前に設定する契約。
    PlayerObject() = default;
    /// 手動 Spawn 用。OnSpawn の自己完結契約に従い、初期位置はコンストラクタで受け取る。
    PlayerObject(float x, float y);

    void OnSpawn() override;
};

} // namespace witch
