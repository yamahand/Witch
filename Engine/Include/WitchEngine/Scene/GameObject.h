#pragma once
#include "WitchEngine/Scene/Component.h"
#include "WitchEngine/Scene/Transform.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace witch {

class Scene;

/// シーン内でオブジェクトを弱参照するための識別子。
using ObjectId = uint64_t;
/// 未設定を示す番兵値。
static constexpr ObjectId kInvalidId = 0;

/// ゲームに登場するすべての実体の基底。振る舞いは Component に分解する。
/// サブクラスは「種別」だけを表し、継承を縦横に広げない。
class GameObject {
public:
    virtual ~GameObject();

    /// スポーン直後に呼ばれる。サブクラスはここに初期化処理を書く。
    virtual void OnSpawn() {}
    /// 毎フレーム全コンポーネントを更新する。サブクラスは追加処理を書いて base を呼ぶ。
    virtual void Update(float dt);
    /// 破棄直前に呼ばれる。
    virtual void OnDespawn() {}

    /// コンポーネントを生成して所有し、OnAttach を呼ぶ。
    /// @tparam T Component の派生クラス
    template<typename T, typename... Args>
    T* AddComponent(Args&&... args);

    /// 型 T の最初のコンポーネントを線形探索で返す。見つからなければ nullptr。
    template<typename T>
    T* GetComponent() const;

    /// 即時 delete せず末尾フラグを立てるだけ。
    /// 更新中の自己破棄でイテレータが壊れないよう、Scene がフレーム末に回収する。
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
