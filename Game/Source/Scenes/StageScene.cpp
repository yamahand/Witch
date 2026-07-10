#include "Scenes/StageScene.h"
#include "Scenes/EmptyScene.h"
#include "WitchEngine/Core/Engine.h"
#include "WitchEngine/Core/Logger.h"
#include "WitchEngine/Core/Services.h"
#include "WitchEngine/Graphics2D/Camera2D.h"
#include "WitchEngine/Graphics2D/CameraManager.h"
#include "WitchEngine/Input/IInput.h"

namespace witch {

namespace {
constexpr const char* kLevelPath = "Stage/Sample1_1.ldtk";
constexpr float kCameraSpeed    = 100.0f;  // カメラ移動 ワールド単位/秒（8px タイル基準）
constexpr float kZoomSpeed      = 3.0f;    // ズーム変化 /秒（Q/E キー）
constexpr float kWheelZoomStep  = 0.5f;    // ホイール 1 ノッチあたりのズーム変化
}

void StageScene::OnEnter() {
    log::Info("StageScene: OnEnter");
    log::Info("keys: WASD=camera Q/E/wheel=zoom G=reload scene Tab=EmptyScene Esc=quit");

    if (auto result = LoadLevel(kLevelPath); !result) {
        log::Error("StageScene: LoadLevel failed: {}", result.error());
        return;
    }

    // レベル全体が縦方向に収まるようカメラを合わせる（中心注視 + ズーム）。
    if (auto* cameras = Services::Instance().cameras) {
        const LevelData* level = CurrentLevel();
        Camera2D& camera = cameras->Active();
        camera.SetPosition(static_cast<float>(level->width) * 0.5f,
                           static_cast<float>(level->height) * 0.5f);
        // ビューポート（仮想解像度）はこの時点で GameLoop が同期済みでない可能性が
        // あるため、レンダラの仮想解像度から直接求める。
        float zoom = 5.0f;
        if (auto* renderer = Services::Instance().renderer; renderer && level->height > 0) {
            zoom = static_cast<float>(renderer->VirtualHeight()) /
                   static_cast<float>(level->height);
        }
        camera.SetZoom(zoom);
    }
}

void StageScene::FixedUpdate(float fixedDt) {
    IInput* input = Services::Instance().input;
    if (input) {
        // WASD でカメラを移動、Q/E でズーム（カリングの動作確認にも使う）。
        if (auto* cameras = Services::Instance().cameras) {
            Camera2D& camera = cameras->Active();
            float dx = 0.0f;
            float dy = 0.0f;
            if (input->IsDown(Key::A)) dx -= 1.0f;
            if (input->IsDown(Key::D)) dx += 1.0f;
            if (input->IsDown(Key::W)) dy -= 1.0f;
            if (input->IsDown(Key::S)) dy += 1.0f;
            camera.Move(dx * kCameraSpeed * fixedDt, dy * kCameraSpeed * fixedDt);

            if (input->IsDown(Key::E)) camera.SetZoom(camera.Zoom() + kZoomSpeed * fixedDt);
            if (input->IsDown(Key::Q)) camera.SetZoom(camera.Zoom() - kZoomSpeed * fixedDt);
        }
    }
    Scene::FixedUpdate(fixedDt);
}

void StageScene::FrameUpdate(float dt) {
    IInput* input = Services::Instance().input;
    if (input) {
        if (input->WasPressed(Key::Escape)) {
            log::Info("Escape pressed — exiting.");
            Engine::Get().RequestExit();
        }

        // G でシーン再入（レベル・テクスチャの解放 → 再ロードの動作確認）。
        if (input->WasPressed(Key::G)) {
            log::Info("G pressed — reloading StageScene.");
            Engine::Get().ChangeScene<StageScene>();
        }

        // Tab で M5 デモ（EmptyScene）へ遷移（シーン間の行き来の確認）。
        if (input->WasPressed(Key::Tab)) {
            log::Info("Tab pressed — switching to EmptyScene.");
            Engine::Get().ChangeScene<EmptyScene>();
        }

        // マウスホイールでズーム（ホイールは瞬間量なのでフレーム側）。
        if (auto* cameras = Services::Instance().cameras) {
            Camera2D& camera = cameras->Active();
            if (float wheel = input->MouseWheelDelta(); wheel != 0.0f)
                camera.SetZoom(camera.Zoom() + wheel * kWheelZoomStep);
        }
    }
    Scene::FrameUpdate(dt);
}

void StageScene::OnExit() {
    log::Info("StageScene: OnExit");
}

} // namespace witch
