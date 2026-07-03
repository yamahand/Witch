#include "WitchEngine/Core/GameLoop.h"
#include "WitchEngine/Core/Services.h"
#include "WitchEngine/Core/Time.h"
#include "WitchEngine/Graphics2D/CameraManager.h"
#include "WitchEngine/Input/IInput.h"
#include "WitchEngine/Rhi/IRenderer.h"
#include "WitchEngine/Scene/Scene.h"
#include "Platform/PlatformWindow.h"
#include "Core/Profiling.h"
#include <cassert>

namespace witch {

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
    // 今フレームのキー／ホイールが current_ に積まれ、Scene::Update での
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
    // SpriteComponent のワールド→スクリーン変換（Scene::Update 内）より前に行う。
    if (CameraManager* cameras = Services::Instance().cameras) {
        cameras->SetViewport(static_cast<float>(renderer_->VirtualWidth()),
                             static_cast<float>(renderer_->VirtualHeight()));
    }

    // 1) 入力を反映しデバッグ UI のフレームを開始（BeginFrame より前に呼べる）。
#ifdef WITCH_DEBUG_UI
    renderer_->BeginDebugUI();
#endif

    // 2) ロジック更新。
    {
        WITCH_PROFILE_SCOPE_N("SceneUpdate");
        if (currentScene) currentScene->Update(time_->DeltaTime());
    }

    // 3) ゲームのデバッグ UI。ImGui フレーム内（BeginDebugUI 後・RenderDebugUI 前）。
#ifdef WITCH_DEBUG_UI
    if (currentScene) {
        WITCH_PROFILE_SCOPE_N("DebugUI");
        currentScene->DrawDebugUI();
    }
#endif

    // 4) 描画。
    {
        WITCH_PROFILE_SCOPE_N("Render");
        auto* cmdList = renderer_->BeginFrame();
        cmdList->Clear({kCornflowerBlue});
        cmdList->FlushSprites();
#ifdef WITCH_DEBUG_UI
        renderer_->RenderDebugUI();
#endif
        renderer_->EndFrame(cmdList);
    }

    return true;
}

} // namespace witch
