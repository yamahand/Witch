#include "WitchEngine/Core/GameLoop.h"
#include "WitchEngine/Core/Services.h"
#include "WitchEngine/Core/Time.h"
#include "WitchEngine/Graphics2D/CameraManager.h"
#include "WitchEngine/Input/IInput.h"
#include "WitchEngine/Rhi/IRenderer.h"
#include "WitchEngine/Scene/Scene.h"
#include "Platform/PlatformWindow.h"
#include "Core/Profiling.h"

namespace witch {

static constexpr rhi::Color kCornflowerBlue{0.392f, 0.584f, 0.929f, 1.0f};

GameLoop::GameLoop(Time* time, IInput* input, rhi::IRenderer* renderer)
    : time_(time), input_(input), renderer_(renderer) {}

bool GameLoop::Tick(Scene* currentScene) {
    WITCH_PROFILE_SCOPE_N("Frame");

    // 入力の世代を進める（previous_ = current_、wheel をリセット）。
    // 必ず PumpMessages の「前」に呼ぶこと。これにより PumpMessages が反映する
    // 今フレームのキー／ホイールが current_ に積まれ、Scene::Update での
    // WasPressed/WasReleased（current vs previous）と MouseWheelDelta が正しく出る。
    // 逆順にすると差分が即座に消えてエッジ検出が常に false になる。
    if (input_) input_->Update();

    {
        WITCH_PROFILE_SCOPE_N("PumpMessages");
        if (!platform::PumpMessages()) {
            return false; // OS が終了メッセージを送ってきた。ループを止める。
        }
    }

    time_->Tick();

    // カメラのビューポートを現在の描画先サイズに同期する。
    // SpriteComponent のワールド→スクリーン変換（Scene::Update 内）より前に行う。
    if (renderer_) {
        if (CameraManager* cameras = Services::Instance().cameras) {
            cameras->SetViewport(static_cast<float>(renderer_->Width()),
                                 static_cast<float>(renderer_->Height()));
        }
    }

    // 1) 入力を反映しデバッグ UI のフレームを開始（BeginFrame より前に呼べる）。
#ifdef WITCH_DEBUG_UI
    if (renderer_) renderer_->BeginDebugUI();
#endif

    // 2) ロジック更新。描画器の有無に依存せず、重複なく 1 か所で行う。
    {
        WITCH_PROFILE_SCOPE_N("SceneUpdate");
        if (currentScene) currentScene->Update(time_->DeltaTime());
    }

    // 3) ゲームのデバッグ UI。ImGui フレーム内（BeginDebugUI 後・RenderDebugUI 前）。
    //    renderer_ が無いときは BeginDebugUI が呼ばれず ImGui フレームが
    //    開始されないため、ゲーム側 ImGui 呼び出しを避けてスキップする。
#ifdef WITCH_DEBUG_UI
    if (renderer_ && currentScene) {
        WITCH_PROFILE_SCOPE_N("DebugUI");
        currentScene->DrawDebugUI();
    }
#endif

    // 4) 描画（描画器がある場合のみ）。
    if (renderer_) {
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
