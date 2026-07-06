#include "WitchEngine/Scene/ComponentScheduler.h"
#include "WitchEngine/Scene/Component.h"
#include "WitchEngine/Scene/GameObject.h"
#include <algorithm>
#include <cassert>

namespace witch {

namespace {
size_t PhaseIndex(UpdatePhase phase) {
    size_t index = static_cast<size_t>(phase);
    // Component::Phase() の実装ミス（UpdatePhase::Count や不正なキャスト値）を
    // 配列外アクセスより前に検出する。
    assert(index < kUpdatePhaseCount && "invalid UpdatePhase");
    return index;
}
} // namespace

void ComponentScheduler::Register(Component* component) {
    pendingAdd_.push_back(component);
}

void ComponentScheduler::UnregisterAll(GameObject* obj) {
    auto ownedBy = [obj](const Component* c) { return c->Owner() == obj; };
    for (auto& list : phases_) {
        std::erase_if(list, ownedBy);
    }
    std::erase_if(pendingAdd_, ownedBy);
}

void ComponentScheduler::RunPhase(UpdatePhase phase, float dt) {
    FlushPending();
    for (Component* c : phases_[PhaseIndex(phase)]) {
        // Register される Component は GameObject::AddComponent 経由でのみ生成され、
        // owner_ を設定済みのはず（Component.h の契約）。
        assert(c->Owner() && "Component registered without an owner");
        if (!c->Owner()->IsDestroyed()) {
            c->Update(dt);
        }
    }
}

void ComponentScheduler::FlushPending() {
    for (Component* c : pendingAdd_) {
        phases_[PhaseIndex(c->Phase())].push_back(c);
    }
    pendingAdd_.clear();
}

} // namespace witch
