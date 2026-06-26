#include "Scenes/EmptyScene.h"
#include "WitchEngine/Core/Logger.h"
#include "WitchEngine/Core/Services.h"
#include "WitchEngine/Core/ResourceManager.h"
#include "WitchEngine/Graphics2D/SpriteComponent.h"

namespace witch {

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
}

void EmptyScene::Update(float dt) {
    ++frameCount_;
    if (frameCount_ % 60 == 1) {
        log::Info("EmptyScene frame {} (dt={:.4f}s)", frameCount_, dt);
    }
    Scene::Update(dt);
}

void EmptyScene::OnExit() {
    log::Info("EmptyScene: OnExit");
}

} // namespace witch
