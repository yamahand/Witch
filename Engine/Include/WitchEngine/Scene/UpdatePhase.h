#pragma once
#include <cstddef>
#include <cstdint>

namespace witch {

/// Component が更新されるフェーズ。宣言順 = フレーム内の実行順。
/// フェーズは「消費者が現れてから」追加する方針（早すぎる一般化をしない）:
/// Physics は M7 で追加済み（消費者 = CollisionComponent）。PrePhysics / PostPhysics は
/// 消費者が現れたら、Input は必要になった時点で足す。
///
/// ## 固定ステップ側 / 毎フレーム側の所属（固定タイムステップ導入後の契約）
/// - PreUpdate / Update / Physics / PostUpdate = **固定ステップ側**（Scene::FixedUpdate）。
///   dt は常に Time::FixedDeltaTime（1/60 秒）。フレーム内で 0〜N 回走る。
/// - Animation / Camera / Render = **毎フレーム側**（Scene::FrameUpdate）。
///   dt は可変のフレーム経過時間。フレームごとに必ず 1 回走る。
///   Render を固定側に置くと 0 ステップフレームで画面が空になるため不可。
enum class UpdatePhase : uint8_t {
    PreUpdate,   ///< 【固定】予約（消費者なし）。他フェーズより先に走らせたい前処理用。
    Update,      ///< 【固定】ゲームロジック（既定）。GameObject::Update フックもこの先頭で走る。
    Physics,     ///< 【固定】速度積分 + 衝突解決（CollisionComponent）。Update で書いた速度が
                 ///< 同一ステップ内で移動へ反映される。エンティティ重なり検出はこの直後に
                 ///< Scene が全体パスとして行う（Scene::FixedUpdate 参照）。
    PostUpdate,  ///< 【固定】予約（消費者なし）。ロジック確定後の後処理用。移動・接触確定後に
                 ///< 同一ステップ内で反応できる位置。
    Animation,   ///< 【毎フレーム】コマ送り・ソース矩形の確定（AnimationComponent / AsepriteComponent）。
    Camera,      ///< 【毎フレーム】カメラ追従・境界クランプ（M8 予定。今は枠のみ）。
    Render,      ///< 【毎フレーム】描画提出（SpriteComponent の SubmitSprite）。実 GPU 描画は GameLoop。
    Count,       ///< 番兵。必ず末尾に置く（kUpdatePhaseCount の自動導出に使う）。
};

inline constexpr size_t kUpdatePhaseCount = static_cast<size_t>(UpdatePhase::Count);

} // namespace witch
