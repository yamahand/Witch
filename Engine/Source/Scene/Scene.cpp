#include "WitchEngine/Scene/Scene.h"
#include "WitchEngine/Core/Logger.h"
#include <algorithm>
#include <atomic>

namespace witch {

namespace {
std::atomic<ObjectId> sNextId{1};
} // namespace

ObjectId Scene::NextId() {
    return sNextId.fetch_add(1, std::memory_order_relaxed);
}

void Scene::Update(float dt) {
    // Stage 1: Reflect pending spawns into the live list.
    // Swap to local first: OnSpawn() may call Spawn(), which pushes to pendingSpawn_.
    // Iterating and push_back-ing the same vector causes reallocation UB.
    // OnSpawn 完了後に spawned_ を立てて components_ を一括登録する。OnSpawn 中の
    // AddComponent は spawned_ が false なので個別登録されず、ここで漏れなく拾われる。
    auto spawning = std::move(pendingSpawn_);
    for (auto& obj : spawning) {
        obj->OnSpawn();
        obj->spawned_ = true;
        for (auto& comp : obj->components_) {
            scheduler_.Register(comp.get());
        }
        objects_.push_back(std::move(obj));
    }

    // Stage 2: Run update phases in declaration order.
    // GameObject::Update フックは Update フェーズの Component より前に呼ぶ。
    scheduler_.RunPhase(UpdatePhase::PreUpdate, dt);
    for (auto& obj : objects_) {
        if (!obj->IsDestroyed()) {
            obj->Update(dt);
        }
    }
    scheduler_.RunPhase(UpdatePhase::Update, dt);
    scheduler_.RunPhase(UpdatePhase::PostUpdate, dt);
    scheduler_.RunPhase(UpdatePhase::Animation, dt);
    scheduler_.RunPhase(UpdatePhase::Camera, dt);
    scheduler_.RunPhase(UpdatePhase::Render, dt);

    // Stage 3: Despawn and collect destroyed objects.
    // スケジューラからの除去は delete（erase_if）より前に行い、dangling を残さない。
    for (auto& obj : objects_) {
        if (obj->IsDestroyed()) {
            scheduler_.UnregisterAll(obj.get());
            obj->OnDespawn();
        }
    }
    std::erase_if(objects_, [](const std::unique_ptr<GameObject>& o) {
        return o->IsDestroyed();
    });
}

#ifdef WITCH_DEBUG_UI
void Scene::DrawDebugUI() {
    for (auto& obj : objects_) {
        if (!obj->IsDestroyed()) obj->DrawDebugUI();
    }
}
#endif

GameObject* Scene::Find(ObjectId id) const {
    for (const auto& obj : objects_) {
        if (obj->Id() == id)
            return obj.get();
    }
    return nullptr;
}

void Scene::LoadLevel(std::string_view /*path*/) {
    log::Warn("Scene::LoadLevel not implemented yet (M6).");
}

} // namespace witch
