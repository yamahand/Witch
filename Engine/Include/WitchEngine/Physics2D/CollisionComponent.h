#pragma once
#include "WitchEngine/Physics2D/Aabb.h"
#include "WitchEngine/Scene/Component.h"
#include "WitchEngine/Scene/GameObject.h"
#include <cstdint>
#include <functional>
#include <vector>

namespace witch {

struct LevelData;
struct LevelIntGrid;
class CollisionComponent;

/// 今ステップの重なり 1 件。ポインタはフレーム末（遅延破棄契約）まで有効。
/// ステップを越えて相手を保持するなら otherId を保存し Scene::Find で解決し直すこと。
struct CollisionContact {
    ObjectId otherId = kInvalidId;
    CollisionComponent* other = nullptr;
};

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
    /// 初回呼び出しで Scene の CollisionWorld へ遅延登録する
    /// （OnAttach 時点では未スポーンで GetScene() が使えない場合があるため）。
    void Update(float dt) override;
    /// CollisionWorld から登録解除する（~GameObject 経由で呼ばれる）。
    void OnDetach() override;

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

    // ── エンティティ同士の重なり（トリガー。押し戻しなし） ──────────────────

    /// 自分の属性ビット（相手の mask と AND される側）。既定 1。
    void SetLayer(uint32_t bits) { layer_ = bits; }
    uint32_t Layer() const { return layer_; }
    /// 当たりたい相手の属性ビット。既定は全ビット（何にでも当たる）。
    /// (自分の mask & 相手の layer) != 0 のとき自分の接触リストに載る（非対称可）。
    void SetMask(uint32_t bits) { mask_ = bits; }
    uint32_t Mask() const { return mask_; }

    /// 今ステップの重なり。CollisionWorld::DetectOverlaps が毎固定ステップ更新する
    /// （次の検出まで有効。受動的に読む一次ソース。PostUpdate フェーズから同一
    /// ステップの接触を読める）。
    const std::vector<CollisionContact>& Contacts() const { return contacts_; }

    /// 重なり通知コールバック（任意）。接触 1 件ごとに、全コライダーの検出が
    /// 完了した後の dispatch 段（Physics 直後・PostUpdate 前）で呼ばれる。
    /// コールバック内の Destroy / Spawn は安全（CollisionWorld.h の契約参照）。
    using OverlapCallback = std::function<void(const CollisionContact&)>;
    void SetOverlapCallback(OverlapCallback callback) {
        overlapCallback_ = std::move(callback);
    }

private:
    friend class CollisionWorld;  ///< contacts_ の書き込みと overlapCallback_ の発火。

    /// Scene のレベルが差し替わっていたら IntGrid を引き直す（LoadLevel 再呼び出し対応）。
    void RefreshGridCache();

    float width_;
    float height_;
    float offsetX_;
    float offsetY_;
    float velX_ = 0.0f;
    float velY_ = 0.0f;
    uint32_t layer_ = 1;
    uint32_t mask_ = ~0u;
    std::vector<CollisionContact> contacts_;
    OverlapCallback overlapCallback_;
    bool registered_ = false;  ///< CollisionWorld へ遅延登録済みか。
    bool solidVsTiles_ = true;
    bool onGround_ = false;
    bool hitHead_ = false;
    bool hitLeft_ = false;
    bool hitRight_ = false;
    const LevelData* cachedLevel_ = nullptr;   ///< 非所有。キャッシュ鍵（ポインタ比較のみ）。
    const LevelIntGrid* grid_ = nullptr;       ///< 非所有。cachedLevel_ 内を指す。
};

} // namespace witch
