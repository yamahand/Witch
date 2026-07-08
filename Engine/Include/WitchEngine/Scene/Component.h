#pragma once
#include "WitchEngine/Scene/DebugUI.h"
#include "WitchEngine/Scene/UpdatePhase.h"

namespace witch {

class GameObject;

/// コンポーネント型を識別する軽量な型 ID。
/// 各型が持つ関数ローカル static のアドレスを使うため、RTTI 不要でリンク跨ぎでも一意。
/// 値の中身に意味はなく、同一性（==）だけを使う。
using ComponentTypeId = const void*;

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

    /// この型を一意に識別する ID。派生では WITCH_COMPONENT マクロが上書きする。
    static ComponentTypeId StaticTypeId() {
        static const int kId = 0;
        return &kId;
    }
    /// 実インスタンスの動的型 ID。GetComponent の完全一致判定に使う（デバッグ用途）。
    virtual ComponentTypeId TypeId() const { return StaticTypeId(); }
    /// このインスタンスが型 ID `id` の型、またはその派生か。
    /// 基底型引き（GetComponent<基底>）を dynamic_cast なしで再現するための判定。
    /// 派生では WITCH_COMPONENT マクロが「自分 or 基底の IsA」で上書きする。
    virtual bool IsA(ComponentTypeId id) const { return id == StaticTypeId(); }

    /// 所有 GameObject を返す。owner は自分より必ず長生きするため生ポインタで持つ。
    GameObject* Owner() const { return owner_; }

private:
    friend class GameObject;
    GameObject* owner_ = nullptr;
};

/// Component 派生クラスの型 ID インフラを宣言する。クラス本体の public 領域に 1 行置く。
/// @param Self このクラス自身の型（現状は使わないが、将来の拡張と可読性のため受ける）
/// @param Base 直接の基底クラス（Component もしくは中間コンポーネント基底）
/// IsA は「自分の ID か、基底の IsA が真か」で答えるため、基底型 ID での取得も成立する。
/// Base が Component 派生でない場合は Base::IsA が解決できずコンパイルエラーになる
/// （Self の完全性を要求する static_assert はクラス本体では評価できないため置かない）。
#define WITCH_COMPONENT(Self, Base)                                          \
    static ::witch::ComponentTypeId StaticTypeId() {                        \
        static const int kId = 0;                                          \
        return &kId;                                                        \
    }                                                                       \
    ::witch::ComponentTypeId TypeId() const override { return StaticTypeId(); } \
    bool IsA(::witch::ComponentTypeId id) const override {                  \
        return id == StaticTypeId() || Base::IsA(id);                       \
    }

} // namespace witch
