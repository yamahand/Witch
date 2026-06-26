#pragma once
#include "WitchEngine/Scene/GameObject.h"
#include <memory>
#include <string_view>
#include <vector>

namespace witch {

class Scene {
public:
    virtual ~Scene() = default;

    virtual void OnEnter() {}
    virtual void OnExit() {}

    // 3-stage update: reflect spawns → update all → collect destroyed.
    // Subclasses may override to add per-frame logic; must call Scene::Update(dt).
    virtual void Update(float dt);

    // Safe to call during Update; the object is added at the start of the next stage.
    template<typename T, typename... Args>
    T* Spawn(Args&&... args);

    // Linear search by id. Returns nullptr if not found.
    GameObject* Find(ObjectId id) const;

    // Instantiates objects from a level file via ObjectRegistry (implemented in M6).
    void LoadLevel(std::string_view path);

private:
    static ObjectId NextId();

    std::vector<std::unique_ptr<GameObject>> objects_;
    std::vector<std::unique_ptr<GameObject>> pendingSpawn_;
};

// ── Template implementation ──────────────────────────────────────────────────

template<typename T, typename... Args>
T* Scene::Spawn(Args&&... args) {
    static_assert(std::is_base_of_v<GameObject, T>,
                  "T must derive from witch::GameObject");
    auto obj = std::make_unique<T>(std::forward<Args>(args)...);
    obj->id_ = NextId();
    obj->scene_ = this;
    T* ptr = obj.get();
    pendingSpawn_.push_back(std::move(obj));
    return ptr;
}

} // namespace witch
