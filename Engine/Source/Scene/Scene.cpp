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
    auto spawning = std::move(pendingSpawn_);
    for (auto& obj : spawning) {
        obj->OnSpawn();
        objects_.push_back(std::move(obj));
    }

    // Stage 2: Update all live objects.
    for (auto& obj : objects_) {
        if (!obj->IsDestroyed()) {
            obj->Update(dt);
        }
    }

    // Stage 3: Despawn and collect destroyed objects.
    for (auto& obj : objects_) {
        if (obj->IsDestroyed()) {
            obj->OnDespawn();
        }
    }
    std::erase_if(objects_, [](const std::unique_ptr<GameObject>& o) {
        return o->IsDestroyed();
    });
}

void Scene::DrawDebugUI() {
    for (auto& obj : objects_) {
        if (!obj->IsDestroyed()) obj->DrawDebugUI();
    }
}

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
