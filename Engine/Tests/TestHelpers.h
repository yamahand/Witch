#pragma once
// テスト共通ヘルパ。固定/毎フレーム分割後の Scene を「1 フレーム」単位で回す。
// 複数のテスト .cpp から include されるためヘッダオンリー（inline）で定義する。
#include "WitchEngine/Core/Time.h"
#include "WitchEngine/Scene/Scene.h"

namespace witch::test {

/// 固定ステップの dt。契約上 FixedUpdate には常にこの値を渡す。
/// 1/60 を重複定義せず単一ソース（Time::kFixedDelta）を参照する。
inline constexpr float kFixedDt = Time::kFixedDelta;

/// 1 フレーム = 固定ステップ 1 回 + フレーム更新 1 回として回すヘルパ。
/// FixedUpdate の dt は契約どおり固定値、FrameUpdate の dt だけ可変にできる。
inline void StepFrame(Scene& scene, float frameDt = kFixedDt) {
    scene.FixedUpdate(kFixedDt);
    scene.FrameUpdate(frameDt);
}

} // namespace witch::test
