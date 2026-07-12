#include "WitchEngine/Core/GameLoop.h"
#include "WitchEngine/Core/Services.h"
#include "WitchEngine/Core/Time.h"
#include "WitchEngine/Debug/DebugDraw.h"
#include "WitchEngine/Graphics2D/CameraManager.h"
#include "WitchEngine/Input/IInput.h"
#include "WitchEngine/Rhi/IRenderer.h"
#include "WitchEngine/Scene/Scene.h"
#include "Platform/PlatformWindow.h"
#include "Core/Profiling.h"
#include <cassert>

#ifdef WITCH_DEBUG_UI
#include "WitchEngine/Debug/DebugMenu.h"
#include "WitchEngine/Debug/HierarchyWindow.h"
#include "WitchEngine/Debug/LogViewerWindow.h"
#endif

namespace witch {

/// シーンが無い間のフォールバック背景色。シーンがあれば Scene::ClearColor()
/// （既定は同色。LoadLevel がレベルの背景色で上書きする）を使う。
static constexpr rhi::Color kCornflowerBlue{0.392f, 0.584f, 0.929f, 1.0f};

GameLoop::GameLoop(Time* time, IInput* input, rhi::IRenderer* renderer)
    : time_(time), input_(input), renderer_(renderer) {
    // Engine::Init が成功した場合のみ生成されるため、依存は常に有効。
    // （以前はレンダラ初期化失敗でも headless で走り続ける設計だったが、
    //  Init が失敗を返すようになったので null 分岐は廃止した。）
    assert(time_ && input_ && renderer_);
}

bool GameLoop::Tick(Scene* currentScene) {
    WITCH_PROFILE_SCOPE_N("Frame");

    // 入力の世代を進める（previous_ = current_、wheel をリセット）。
    // 必ず PumpMessages の「前」に呼ぶこと。これにより PumpMessages が反映する
    // 今フレームのキー／ホイールが current_ に積まれ、Scene::FrameUpdate での
    // WasPressed/WasReleased（current vs previous）と MouseWheelDelta が正しく出る。
    // 逆順にすると差分が即座に消えてエッジ検出が常に false になる。
    input_->Update();

    {
        WITCH_PROFILE_SCOPE_N("PumpMessages");
        if (!platform::PumpMessages()) {
            return false; // OS が終了メッセージを送ってきた。ループを止める。
        }
    }

    time_->Tick();

    // カメラのビューポートを仮想解像度（無効時はウィンドウ実サイズ）に同期する。
    // これにより「画面に見えるワールド範囲」がウィンドウサイズと切り離される。
    // ScreenToWorld 系の CPU 変換（マウスピック等）がシーン更新
    // （FixedUpdate / FrameUpdate）内で正しく動くよう、更新より前に行う。
    if (CameraManager* cameras = Services::Instance().cameras) {
        cameras->SetViewport(static_cast<float>(renderer_->VirtualWidth()),
                             static_cast<float>(renderer_->VirtualHeight()));
    }

    // 1) 入力を反映しデバッグ UI のフレームを開始（BeginFrame より前に呼べる）。
#ifdef WITCH_DEBUG_UI
    renderer_->BeginDebugUI();
#endif

    // 2) ロジック更新。固定タイムステップ（アキュムレータ方式）:
    //    固定ステップ（60Hz、フレーム内 0〜N 回）→ フレーム更新（必ず 1 回）。
    //    ステップ数の上界は Time::Tick の kMaxDelta クランプが与える（最大 ~15 回）。
    //    エッジ入力（WasPressed）は input_->Update() がフレームに 1 回のため、
    //    固定側で読むと多重ステップフレームで二重発火する。FrameUpdate 側で読むこと。
    {
        WITCH_PROFILE_SCOPE_N("SceneUpdate");
        // シーンが無い間もステップは消費する（アキュムレータに溜め込んで
        // シーン設定直後にまとめて走るのを防ぐ）。
        debug::DebugDraw* debugDraw = Services::Instance().debugDraw;
        while (time_->ConsumeFixedStep()) {
            if (currentScene) {
                WITCH_PROFILE_SCOPE_N("FixedStep");
                // ステップごとに固定側デバッグ描画を積み直す（DebugDraw.h の契約参照）。
                if (debugDraw) debugDraw->BeginFixedStep();
                currentScene->FixedUpdate(time_->FixedDeltaTime());
            }
        }
        if (debugDraw) debugDraw->EndFixedSteps();
        if (currentScene) currentScene->FrameUpdate(time_->DeltaTime());
    }

    // カメラのビュー変換を RHI に渡す（World スプライトに VS で適用される）。
    // Camera フェーズまでの更新で確定した今フレームの値を、描画前にここで送る。
    // カメラ未設定なら恒等（ワールド座標をそのままスクリーン座標として扱う）。
    if (CameraManager* cameras = Services::Instance().cameras) {
        const Camera2D& cam = cameras->Active();
        renderer_->SetCamera(cam.ViewScale(), cam.ViewOffsetX(), cam.ViewOffsetY());
    } else {
        renderer_->SetCamera(1.0f, 0.0f, 0.0f);
    }

    // 3) デバッグ UI。ImGui フレーム内（BeginDebugUI 後・RenderDebugUI 前）。
#ifdef WITCH_DEBUG_UI
    {
        WITCH_PROFILE_SCOPE_N("DebugUI");
        if (currentScene) currentScene->DrawDebugUI();
        if (hierarchyWindow_) hierarchyWindow_->Draw(currentScene); // ヒエラルキー＋インスペクター
        if (logViewer_) logViewer_->Draw(); // エンジン標準の Log Viewer
        // デバッグウィンドウ外を右クリックしたときのコンテキストメニュー。
        // BeginPopupContextVoid はどの ImGui ウィンドウ上でもないかを内部で判定するため、
        // 他ウィンドウの描画より前後どちらでもよいが、並びを一貫させるため最後に呼ぶ。
        if (debugMenu_) debugMenu_->Draw();
    }
#endif

    // デバッグプリミティブを RHI へ提出する。DrawDebugUI（インスペクター等）からの
    // 提出も拾えるよう、デバッグ UI の後・描画の前に行う。
    if (debug::DebugDraw* debugDraw = Services::Instance().debugDraw) {
        debugDraw->Flush();
    }

    // 4) 描画。
    {
        WITCH_PROFILE_SCOPE_N("Render");
        auto* cmdList = renderer_->BeginFrame();
        cmdList->Clear({currentScene ? currentScene->ClearColor() : kCornflowerBlue});
        cmdList->FlushSprites();
#ifdef WITCH_DEBUG_DRAW
        // デバッグ線分は全スプライトの手前・ImGui（RenderDebugUI）の奥に描く。
        cmdList->FlushLines();
#endif
#ifdef WITCH_DEBUG_UI
        renderer_->RenderDebugUI();
#endif
        renderer_->EndFrame(cmdList);
    }

    return true;
}

} // namespace witch
