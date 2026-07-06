#include "WitchEngine/Scene/GameObject.h"
#include "WitchEngine/Scene/Scene.h"

namespace witch {

GameObject::~GameObject() {
    // Detach components in reverse attachment order.
    for (auto it = components_.rbegin(); it != components_.rend(); ++it) {
        (*it)->OnDetach();
    }
}

void GameObject::Update(float /*dt*/) {
    // オブジェクト単位のフック。既定では何もしない。
    // Component の更新は Scene の ComponentScheduler がフェーズ順に行う。
}

void GameObject::RegisterComponent(Component* component) {
    if (spawned_ && scene_) {
        scene_->scheduler_.Register(component);
    }
}

#ifdef WITCH_DEBUG_UI
void GameObject::DrawDebugUI() {
    for (auto& comp : components_) {
        comp->DrawDebugUI();
    }
}
#endif

} // namespace witch
