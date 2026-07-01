#include "Scenes/EmptyScene.h"
#include "WitchEngine/Core/Engine.h"
#include "WitchEngine/Core/Logger.h"
#include "WitchEngine/Core/Services.h"
#include "WitchEngine/Core/ResourceManager.h"
#include "WitchEngine/Graphics2D/Camera2D.h"
#include "WitchEngine/Graphics2D/CameraManager.h"
#include "WitchEngine/Graphics2D/SpriteComponent.h"
#include "WitchEngine/Input/IInput.h"

namespace witch {

namespace {
constexpr float kMoveSpeed     = 300.0f; // スプライト移動 ピクセル/秒（ワールド）
constexpr float kCameraSpeed   = 400.0f; // カメラ移動 ワールド単位/秒
constexpr float kZoomSpeed     = 1.5f;   // ズーム変化 /秒（Q/E キー）
constexpr float kWheelZoomStep = 0.1f;   // ホイール 1 ノッチあたりのズーム変化
}

void EmptyScene::OnEnter() {
    log::Info("EmptyScene: OnEnter");

    // Load test sprite texture.
    auto result = Services::Instance().resources->LoadTexture("Assets/Witch.png");
    if (!result) {
        log::Error("Failed to load sprite: {}", result.error());
        return;
    }
    spriteTexture_ = *result;

    // Witch をワールド原点 (0,0) に配置（128x128 の左上が原点）。
    auto* obj = Spawn<GameObject>();
    obj->transform.x = 0.0f;
    obj->transform.y = 0.0f;
    obj->AddComponent<SpriteComponent>(spriteTexture_, 128.0f, 128.0f);
    witchId_ = obj->Id(); // Update から弱参照で解決する。

    // カメラを Witch の中心あたりに向ける（スプライト左上が原点なので +64 で中心）。
    Camera2D& camera = Services::Instance().cameras->Active();
    camera.SetPosition(64.0f, 64.0f);
    camera.SetZoom(1.0f);
}

void EmptyScene::Update(float dt) {
    ++frameCount_;

    IInput* input = Services::Instance().input;
    if (input) {
        // Escape でアプリ終了（WasPressed のエッジ検出を確認）。
        if (input->WasPressed(Key::Escape)) {
            log::Info("Escape pressed — exiting.");
            Engine::Get().RequestExit();
        }

        // 矢印キーで Witch スプライトを移動（ワールド座標, IsDown の継続入力）。
        // 注: OnEnter の Spawn は pendingSpawn_ 行きのため、最初の Update フレームでは
        // まだ objects_ に反映されておらず Find は nullptr を返す（移動が1フレーム遅れる）。
        // null チェックで安全にスキップされ、デモ用途では実害なし。
        if (GameObject* witch = Find(witchId_)) {
            float dx = 0.0f;
            float dy = 0.0f;
            if (input->IsDown(Key::Left))  dx -= 1.0f;
            if (input->IsDown(Key::Right)) dx += 1.0f;
            if (input->IsDown(Key::Up))    dy -= 1.0f;
            if (input->IsDown(Key::Down))  dy += 1.0f;
            witch->transform.x += dx * kMoveSpeed * dt;
            witch->transform.y += dy * kMoveSpeed * dt;
        }

        // WASD でカメラを移動、Q/E でズーム（カメラ／座標系の動作確認）。
        Camera2D& camera = Services::Instance().cameras->Active();
        float cdx = 0.0f;
        float cdy = 0.0f;
        if (input->IsDown(Key::A)) cdx -= 1.0f;
        if (input->IsDown(Key::D)) cdx += 1.0f;
        if (input->IsDown(Key::W)) cdy -= 1.0f;
        if (input->IsDown(Key::S)) cdy += 1.0f;
        camera.Move(cdx * kCameraSpeed * dt, cdy * kCameraSpeed * dt);

        if (input->IsDown(Key::E)) camera.SetZoom(camera.Zoom() + kZoomSpeed * dt);
        if (input->IsDown(Key::Q)) camera.SetZoom(camera.Zoom() - kZoomSpeed * dt);
        // マウスホイールでもズーム。
        if (float wheel = input->MouseWheelDelta(); wheel != 0.0f)
            camera.SetZoom(camera.Zoom() + wheel * kWheelZoomStep);
    }

    if (frameCount_ % 60 == 1) {
        log::Info("EmptyScene frame {} (dt={:.4f}s) mouse=({:.0f},{:.0f})",
                  frameCount_, dt,
                  input ? input->MouseX() : 0.0f,
                  input ? input->MouseY() : 0.0f);
    }
    Scene::Update(dt);
}

void EmptyScene::OnExit() {
    log::Info("EmptyScene: OnExit");
}

} // namespace witch
