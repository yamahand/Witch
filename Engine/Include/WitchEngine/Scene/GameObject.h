#pragma once
#include "WitchEngine/Scene/Component.h"
#include "WitchEngine/Scene/Transform.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace witch {

class Scene;

using ObjectId = uint64_t;
static constexpr ObjectId kInvalidId = 0;

class GameObject {
public:
    virtual ~GameObject();

    virtual void OnSpawn() {}
    virtual void Update(float dt);
    virtual void OnDespawn() {}

    template<typename T, typename... Args>
    T* AddComponent(Args&&... args);

    template<typename T>
    T* GetComponent() const;

    // Flags this object for deferred destruction at end of frame.
    void Destroy() { pendingDestroy_ = true; }
    bool IsDestroyed() const { return pendingDestroy_; }

    ObjectId Id() const { return id_; }
    Scene* GetScene() const { return scene_; }

    Transform transform;

private:
    friend class Scene;

    ObjectId id_ = kInvalidId;
    Scene* scene_ = nullptr;
    bool pendingDestroy_ = false;
    std::vector<std::unique_ptr<Component>> components_;
};

// ── Template implementations ────────────────────────────────────────────────

template<typename T, typename... Args>
T* GameObject::AddComponent(Args&&... args) {
    static_assert(std::is_base_of_v<Component, T>,
                  "T must derive from witch::Component");
    auto comp = std::make_unique<T>(std::forward<Args>(args)...);
    comp->owner_ = this;
    comp->OnAttach();
    T* ptr = comp.get();
    components_.push_back(std::move(comp));
    return ptr;
}

template<typename T>
T* GameObject::GetComponent() const {
    for (const auto& comp : components_) {
        if (T* c = dynamic_cast<T*>(comp.get()))
            return c;
    }
    return nullptr;
}

} // namespace witch
