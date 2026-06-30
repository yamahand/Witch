#include "WitchEngine/Scene/GameObject.h"

namespace witch {

GameObject::~GameObject() {
    // Detach components in reverse attachment order.
    for (auto it = components_.rbegin(); it != components_.rend(); ++it) {
        (*it)->OnDetach();
    }
}

void GameObject::Update(float dt) {
    for (auto& comp : components_) {
        comp->Update(dt);
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
