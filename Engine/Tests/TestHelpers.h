#pragma once
// テスト共通ヘルパ。固定/毎フレーム分割後の Scene を「1 フレーム」単位で回す。
// 複数のテスト .cpp から include されるためヘッダオンリー（inline）で定義する。
#include "WitchEngine/Core/Services.h"
#include "WitchEngine/Core/Time.h"
#include "WitchEngine/Scene/Scene.h"
#include "WitchEngine/Vfs/Vfs.h"
#include <catch2/catch_test_macros.hpp>

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

/// Engine/Tests/Fixtures/ をマウントした VFS を Services に差し込み、
/// スコープ終了時に元へ戻す（他テストへの影響を残さない）。
struct ScopedFixtureVfs {
    vfs::Vfs vfs;
    vfs::Vfs* prev;
    ScopedFixtureVfs() {
        REQUIRE(vfs.MountDisk(WITCH_TEST_FIXTURE_DIR).has_value());
        vfs.Seal();
        prev = Services::Instance().vfs;
        Services::Instance().vfs = &vfs;
    }
    ~ScopedFixtureVfs() { Services::Instance().vfs = prev; }
};

} // namespace witch::test
