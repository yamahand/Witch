// GetComponent の型 ID タグ方式（Component.h の WITCH_COMPONENT / IsA）の契約テスト。
// dynamic_cast 廃止後も「基底型引き」が成立すること、型 ID がリンク後も型ごとに
// 一意であること（Release の /OPT:ICF 畳み込み回帰の検出）を固定する。
#include "WitchEngine/Scene/Component.h"
#include "WitchEngine/Scene/GameObject.h"

#include <catch2/catch_test_macros.hpp>

namespace {

using namespace witch;

// 中身が同一（空）の 2 型。/OPT:ICF が型 ID を畳み込むと StaticTypeId が一致してしまう。
class CompA : public Component {
public:
    WITCH_COMPONENT(CompA, Component)
};

class CompB : public Component {
public:
    WITCH_COMPONENT(CompB, Component)
};

class DerivedA : public CompA {
public:
    WITCH_COMPONENT(DerivedA, CompA)
};

TEST_CASE("StaticTypeId is unique per type", "[Component]") {
    // 空クラス同士でも ID は畳み込まれない（kId が非 const な理由。Component.h 参照）。
    CHECK(CompA::StaticTypeId() != CompB::StaticTypeId());
    CHECK(CompA::StaticTypeId() != DerivedA::StaticTypeId());
    CHECK(CompA::StaticTypeId() != Component::StaticTypeId());
    // 同じ型からは常に同じ ID。
    CHECK(CompA::StaticTypeId() == CompA::StaticTypeId());
}

TEST_CASE("IsA answers self and ancestors only", "[Component]") {
    DerivedA derived;
    CHECK(derived.IsA(DerivedA::StaticTypeId()));
    CHECK(derived.IsA(CompA::StaticTypeId()));
    CHECK(derived.IsA(Component::StaticTypeId()));
    CHECK_FALSE(derived.IsA(CompB::StaticTypeId()));

    CompA base;
    CHECK_FALSE(base.IsA(DerivedA::StaticTypeId()));  // 基底は派生の ID に答えない
}

TEST_CASE("GetComponent finds exact type", "[Component]") {
    GameObject obj;
    auto* added = obj.AddComponent<CompA>();

    CHECK(obj.GetComponent<CompA>() == added);
    CHECK(obj.GetComponent<CompB>() == nullptr);
}

TEST_CASE("GetComponent finds derived instance via base type", "[Component]") {
    GameObject obj;
    auto* derived = obj.AddComponent<DerivedA>();

    // 基底型引き: GetComponent<CompA> が DerivedA インスタンスを返す（IsA チェーン）。
    CHECK(obj.GetComponent<CompA>() == derived);
    CHECK(obj.GetComponent<DerivedA>() == derived);
}

TEST_CASE("AddComponent calls OnAttach and sets Owner", "[Component]") {
    class AttachProbe : public Component {
    public:
        WITCH_COMPONENT(AttachProbe, Component)
        void OnAttach() override { attachedOwner = Owner(); }
        GameObject* attachedOwner = nullptr;
    };

    GameObject obj;
    auto* comp = obj.AddComponent<AttachProbe>();

    // OnAttach 時点で Owner が設定済み。
    CHECK(comp->attachedOwner == &obj);
}

}  // namespace
