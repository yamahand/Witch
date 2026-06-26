#include "Scenes/EmptyScene.h"
#include "WitchEngine/Core/Logger.h"

namespace witch {

void EmptyScene::OnEnter() {
    log::Info("EmptyScene: OnEnter");
}

void EmptyScene::Update(float dt) {
    ++frameCount_;
    // Log every 60 frames to avoid flooding the console.
    if (frameCount_ % 60 == 1) {
        log::Info("EmptyScene frame {} (dt={:.4f}s)", frameCount_, dt);
    }
    Scene::Update(dt);
}

void EmptyScene::OnExit() {
    log::Info("EmptyScene: OnExit");
}

} // namespace witch
