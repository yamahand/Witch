#pragma once
#include <cstddef>
#include <cstdint>

namespace witch {

/// Component が更新されるフェーズ。宣言順 = フレーム内の実行順。
/// フェーズは「消費者が現れてから」追加する方針（早すぎる一般化をしない）:
/// Physics 系（PrePhysics / Physics / PostPhysics）は M7、Input は必要になった時点で足す。
///
/// ## 固定ステップ側 / 毎フレーム側の所属（固定タイムステップ導入後の契約）
/// - PreUpdate / Update / PostUpdate = **固定ステップ側**（Scene::FixedUpdate）。
///   dt は常に Time::FixedDeltaTime（1/60 秒）。フレーム内で 0〜N 回走る。
///   M7 の Physics 系フェーズはこちらに足す。
/// - Animation / Camera / Render = **毎フレーム側**（Scene::FrameUpdate）。
///   dt は可変のフレーム経過時間。フレームごとに必ず 1 回走る。
///   Render を固定側に置くと 0 ステップフレームで画面が空になるため不可。
enum class UpdatePhase : uint8_t {
    PreUpdate,   ///< 【固定】予約（消費者なし）。他フェーズより先に走らせたい前処理用。
    Update,      ///< 【固定】ゲームロジック（既定）。GameObject::Update フックもこの先頭で走る。
    PostUpdate,  ///< 【固定】予約（消費者なし）。ロジック確定後の後処理用。
    Animation,   ///< 【毎フレーム】コマ送り・ソース矩形の確定（AnimationComponent / AsepriteComponent）。
    Camera,      ///< 【毎フレーム】カメラ追従・境界クランプ（M8 予定。今は枠のみ）。
    Render,      ///< 【毎フレーム】描画提出（SpriteComponent の SubmitSprite）。実 GPU 描画は GameLoop。
    Count,       ///< 番兵。必ず末尾に置く（kUpdatePhaseCount の自動導出に使う）。
};

inline constexpr size_t kUpdatePhaseCount = static_cast<size_t>(UpdatePhase::Count);

} // namespace witch
