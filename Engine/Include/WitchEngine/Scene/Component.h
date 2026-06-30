#pragma once
#include "WitchEngine/Scene/DebugUI.h"

namespace witch {

class GameObject;

/// すべてのコンポーネントの基底。振る舞いを GameObject から分離する単位。
/// DrawDebugUI() は DebugUI 基底から継承する（WITCH_DEBUG_UI 定義時のみ存在）。
class Component : public DebugUI {
public:
    virtual ~Component() = default;

    /// AddComponent 完了直後に呼ばれる。初期化処理を書く。
    virtual void OnAttach() {}
    /// 毎フレーム GameObject::Update から呼ばれる。
    virtual void Update([[maybe_unused]] float dt) {}
    /// コンポーネント破棄直前に呼ばれる。
    virtual void OnDetach() {}

    /// 所有 GameObject を返す。owner は自分より必ず長生きするため生ポインタで持つ。
    GameObject* Owner() const { return owner_; }

private:
    friend class GameObject;
    GameObject* owner_ = nullptr;
};

} // namespace witch
