#pragma once
#include "WitchEngine/Physics2D/Aabb.h"
#include "WitchEngine/Scene/Component.h"

namespace witch {

struct LevelData;
struct LevelIntGrid;

/// AABB のキネマティックボディ。velocity を保持し、Physics フェーズ（固定側）で
/// 速度積分 + タイル押し戻しを自動的に行って owner の transform に書き戻す。
/// ゲームロジック（Update フェーズのコントローラ等）は速度の読み書きだけで
/// 移動を制御する（ジャンプ = SetVelocity で vy を上書き）。
///
/// - AABB は owner の transform を**中心**に置く（SpriteComponent の既定アンカー
///   Center と整合）。offset で中心からずらせる（足元寄せ等）。
/// - 衝突するタイルは Scene::CurrentLevel() の先頭 IntGrid
///   （physics2d::FindCollisionGrid の規約）。レベル未ロードなら素通し。
///   LoadLevel の再呼び出しでレベルが差し替わっても安全なよう、IntGrid は
///   レベルポインタ比較付きで遅延キャッシュする（毎ステップの比較 1 回のみ）。
/// - 座標系は y-down: OnGround = +Y（下）移動が遮られた、HitHead = -Y（上）。
class CollisionComponent : public Component {
public:
    WITCH_COMPONENT(CollisionComponent, Component);

    /// @param width / height AABB のサイズ px
    /// @param offsetX / offsetY transform（中心）からの AABB 中心のずらし量 px
    CollisionComponent(float width, float height,
                       float offsetX = 0.0f, float offsetY = 0.0f);

    UpdatePhase Phase() const override { return UpdatePhase::Physics; }
    /// 速度積分 + タイル押し戻し + transform 書き戻し。遮られた軸の速度成分は
    /// 0 になる（着地で落下停止、壁で水平停止の標準挙動）。
    void Update(float dt) override;

    void SetVelocity(float vx, float vy) { velX_ = vx; velY_ = vy; }
    void SetVelocityX(float vx) { velX_ = vx; }
    void SetVelocityY(float vy) { velY_ = vy; }
    float VelocityX() const { return velX_; }
    float VelocityY() const { return velY_; }

    /// 直近の Physics ステップの押し戻し結果（次のステップで上書きされる）。
    bool OnGround() const { return onGround_; }
    bool HitHead() const { return hitHead_; }
    bool HitWall() const { return hitLeft_ || hitRight_; }
    bool HitLeft() const { return hitLeft_; }
    bool HitRight() const { return hitRight_; }

    /// false にするとタイルと衝突しない（すり抜けるトリガー・アイテム等）。
    /// 速度積分と transform 書き戻しは行われる。
    void SetSolidVsTiles(bool solid) { solidVsTiles_ = solid; }
    bool SolidVsTiles() const { return solidVsTiles_; }

    /// 現在の transform + offset / サイズから算出したワールド AABB（左上 + サイズ）。
    Aabb WorldAabb() const;

private:
    /// Scene のレベルが差し替わっていたら IntGrid を引き直す（LoadLevel 再呼び出し対応）。
    void RefreshGridCache();

    float width_;
    float height_;
    float offsetX_;
    float offsetY_;
    float velX_ = 0.0f;
    float velY_ = 0.0f;
    bool solidVsTiles_ = true;
    bool onGround_ = false;
    bool hitHead_ = false;
    bool hitLeft_ = false;
    bool hitRight_ = false;
    const LevelData* cachedLevel_ = nullptr;   ///< 非所有。キャッシュ鍵（ポインタ比較のみ）。
    const LevelIntGrid* grid_ = nullptr;       ///< 非所有。cachedLevel_ 内を指す。
};

} // namespace witch
