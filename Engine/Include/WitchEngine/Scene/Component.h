#pragma once

namespace witch {

class GameObject;

class Component {
public:
    virtual ~Component() = default;

    virtual void OnAttach() {}
    virtual void Update([[maybe_unused]] float dt) {}
    virtual void OnDetach() {}

    GameObject* Owner() const { return owner_; }

private:
    friend class GameObject;
    GameObject* owner_ = nullptr;
};

} // namespace witch
