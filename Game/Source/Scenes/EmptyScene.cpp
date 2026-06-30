#include "Scenes/EmptyScene.h"
#include "WitchEngine/Core/Engine.h"
#include "WitchEngine/Core/Logger.h"
#include "WitchEngine/Core/Services.h"
#include "WitchEngine/Core/ResourceManager.h"
#include "WitchEngine/Graphics2D/SpriteComponent.h"
#include "WitchEngine/Input/IInput.h"

namespace witch {

namespace {
constexpr float kMoveSpeed = 300.0f; // ピクセル/秒
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

    // Spawn a GameObject with a SpriteComponent at screen position (100, 100).
    auto* obj = Spawn<GameObject>();
    obj->transform.x = 100.0f;
    obj->transform.y = 100.0f;
    obj->AddComponent<SpriteComponent>(spriteTexture_, 128.0f, 128.0f);
    witchId_ = obj->Id(); // Update から弱参照で解決する。
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

        // 矢印キーで Witch スプライトを移動（IsDown の継続入力）。
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
