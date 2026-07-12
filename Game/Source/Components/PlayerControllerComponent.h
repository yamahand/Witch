#pragma once
#include "WitchEngine/Scene/Component.h"

namespace witch {

class CollisionComponent;
class SpriteComponent;

/// プレイヤーのキャラクターコントローラ（重力・左右移動・ジャンプ）。
/// エンジンは衝突解決（CollisionComponent）までを提供し、手触りはここで作る
/// （CLAUDE.md / RemainingWork §5 の役割分担）。Update フェーズ（固定側）で入力を
/// 読み速度を書き、同一ステップの Physics フェーズが積分 + 押し戻しする。
///
/// 操作（暫定。M9 のアクションマッピングで置換）: ←→ = 移動、Z = ジャンプ。
class PlayerControllerComponent : public Component {
public:
    WITCH_COMPONENT(PlayerControllerComponent, Component);

    /// 兄弟の CollisionComponent（必須）と SpriteComponent（任意、向き反転用）を
    /// キャッシュする。**Collision / Sprite を先に AddComponent しておくこと**
    /// （PlayerObject::OnSpawn の順序契約）。
    void OnAttach() override;
    void Update(float dt) override;

#ifdef WITCH_DEBUG_UI
    /// 調整定数のライブ編集（手触り調整用）。
    void DrawInspector() override;
#endif

private:
    CollisionComponent* collision_ = nullptr;
    SpriteComponent* sprite_ = nullptr;

    /// ジャンプ入力の自前エッジ検出用。WasPressed（フレーム世代）を固定ステップで
    /// 読むと多重ステップフレームで二重発火するため、IsDown のレベル値を毎ステップ
    /// 標本化して立ち上がり/立ち下がりを取る（入力状態はフレーム内で不変なので、
    /// 多重ステップでもエッジは最初の 1 ステップでのみ真になる）。
    /// 既知の限界: 1 フレーム未満のタップ（押して離すが同一フレーム内）は取りこぼす。
    /// 60fps では実質問題にならず、問題化したら固定ステップ入力スナップショットを
    /// エンジン側に設計する（RemainingWork §3 の注記）。
    bool prevJumpHeld_ = false;

    // ── 調整定数（8px タイル基準の初期値。DrawInspector でライブ調整して確定する） ──
    float moveSpeed_ = 50.0f;      ///< 最高水平速度 px/s
    float groundAccel_ = 600.0f;   ///< 地上加速度 px/s²
    float airAccel_ = 300.0f;      ///< 空中加速度 px/s²（空中制御は地上より鈍く）
    float gravity_ = 700.0f;       ///< 重力加速度 px/s²（y-down なので +Y 向き）
    float maxFallSpeed_ = 240.0f;  ///< 落下の終端速度 px/s
    float jumpSpeed_ = -165.0f;    ///< ジャンプ初速 px/s（上 = -Y。約 2.4 タイル跳躍）
    float jumpCutFactor_ = 0.5f;   ///< 上昇中にボタンを離したときの vy 減衰率（可変ジャンプ高）
};

} // namespace witch
