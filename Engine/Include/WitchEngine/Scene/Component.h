#pragma once
#include "WitchEngine/Scene/DebugUI.h"
#include "WitchEngine/Scene/UpdatePhase.h"
#include <type_traits>

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
    /// マクロ適用済みかの検出用マーカー（kHasComponentTypeId 参照）。
    /// WITCH_COMPONENT が各型で自分自身に再定義する。付け忘れると親のものが継承され、
    /// `ComponentSelfType != 自分の型` になるので多段継承でも付け忘れを検出できる。
    using ComponentSelfType = Component;
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
///
/// このマクロは `StaticTypeId()` をクラスごとに再定義し、`ComponentSelfType` を
/// Self 自身に束ねる。後者により kHasComponentTypeId が「その型自身がマクロを適用したか」を
/// 判定でき、多段継承（B : A で B がマクロ付け忘れ）でも付け忘れを検出できる。
#define WITCH_COMPONENT(Self, Base)                                          \
    using ComponentSelfType = Self;                                          \
    static ::witch::ComponentTypeId StaticTypeId() {                        \
        static const int kId = 0;                                          \
        return &kId;                                                        \
    }                                                                       \
    bool IsA(::witch::ComponentTypeId id) const override {                  \
        return id == StaticTypeId() || Base::IsA(id);                       \
    }

/// 型 T が WITCH_COMPONENT を「T 自身で」適用済みか。
/// マクロは `ComponentSelfType = Self` を定義するので、適用済みなら
/// `T::ComponentSelfType == T` になる。付け忘れると親の ComponentSelfType が継承され
/// （例: B : A で B が付け忘れ → B::ComponentSelfType == A ≠ B）、多段継承でも検出できる。
/// これによりマクロ付け忘れのまま GetComponent して誤った static_cast（未定義動作）に
/// なる後退を、GetComponent の static_assert で機械的に弾く。
template<typename T>
inline constexpr bool kHasComponentTypeId =
    std::is_same_v<typename T::ComponentSelfType, T>;

} // namespace witch
