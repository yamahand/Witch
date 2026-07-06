#pragma once
#include <cstddef>
#include <cstdint>

namespace witch {

/// Component が更新されるフェーズ。宣言順 = フレーム内の実行順。
/// フェーズは「消費者が現れてから」追加する方針（早すぎる一般化をしない）:
/// Physics 系（PrePhysics / Physics / PostPhysics）は M7、Input は必要になった時点で足す。
enum class UpdatePhase : uint8_t {
    PreUpdate,   ///< 予約（消費者なし）。他フェーズより先に走らせたい前処理用。
    Update,      ///< ゲームロジック（既定）。GameObject::Update フックもこの先頭で走る。
    PostUpdate,  ///< 予約（消費者なし）。ロジック確定後の後処理用。
    Animation,   ///< コマ送り・ソース矩形の確定（AnimationComponent / AsepriteComponent）。
    Camera,      ///< カメラ追従・境界クランプ（M8 予定。今は枠のみ）。
    Render,      ///< 描画提出（SpriteComponent の SubmitSprite）。実 GPU 描画は GameLoop。
};

inline constexpr size_t kUpdatePhaseCount = 6;

} // namespace witch
