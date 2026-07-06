#pragma once
#include "WitchEngine/Scene/DebugUI.h"
#include "WitchEngine/Scene/UpdatePhase.h"

namespace witch {

class GameObject;

/// すべてのコンポーネントの基底。振る舞いを GameObject から分離する単位。
/// DrawDebugUI() は DebugUI 基底から継承する（WITCH_DEBUG_UI 定義時のみ存在）。
class Component : public DebugUI {
public:
    virtual ~Component() = default;

    /// AddComponent 完了直後に呼ばれる。初期化処理を書く。
    virtual void OnAttach() {}
    /// 毎フレーム、Phase() が返すフェーズで ComponentScheduler から呼ばれる。
    /// 同一フェーズ内の GameObject 間の実行順は未規定（ComponentScheduler.h の契約参照）。
    virtual void Update([[maybe_unused]] float dt) {}
    /// コンポーネント破棄直前に呼ばれる。
    virtual void OnDetach() {}

    /// 所属する更新フェーズ。種類ごとに固定（既定は Update）。
    /// インスタンスの生存中に返す値を変えてはならない（登録先リストが変わらないため）。
    virtual UpdatePhase Phase() const { return UpdatePhase::Update; }

    /// 所有 GameObject を返す。owner は自分より必ず長生きするため生ポインタで持つ。
    GameObject* Owner() const { return owner_; }

private:
    friend class GameObject;
    GameObject* owner_ = nullptr;
};

} // namespace witch
