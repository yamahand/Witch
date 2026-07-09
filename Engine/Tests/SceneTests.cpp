// Scene の 3 段階更新（生成反映 → フェーズ実行 → 破棄回収）の契約テスト。
// 仕様は Scene.h / GameObject.h のコメントに文書化されたもの（順序厳守・遅延 Spawn・遅延破棄）。
#include "WitchEngine/Scene/Component.h"
#include "WitchEngine/Scene/GameObject.h"
#include "WitchEngine/Scene/Scene.h"

#include <catch2/catch_test_macros.hpp>

namespace {

using namespace witch;

/// ライフサイクルフックの呼び出し回数を数える GameObject。
class ProbeObject : public GameObject {
public:
    void OnSpawn() override { ++spawnCount; }
    void OnDespawn() override { ++despawnCount; }
    void Update(float) override { ++updateCount; }

    int spawnCount = 0;
    int despawnCount = 0;
    int updateCount = 0;
};

/// Update 回数を数えるだけの Component（既定フェーズ = Update）。
class CountingComponent : public Component {
public:
    WITCH_COMPONENT(CountingComponent, Component)
    void Update(float) override { ++updateCount; }
    int updateCount = 0;
};

/// 外部カウンタに Update 回数を書く Component。
/// Owner ごと delete された後も回数を検証できる（ポインタ経由で読むと解放済みメモリになる）。
class ExternalCountingComponent : public Component {
public:
    WITCH_COMPONENT(ExternalCountingComponent, Component)
    explicit ExternalCountingComponent(int* counter) : counter_(counter) {}
    void Update(float) override { ++(*counter_); }

private:
    int* counter_;
};

/// 最初の Update で自分の Owner を Destroy する GameObject。
class SelfDestroyObject : public ProbeObject {
public:
    void Update(float dt) override {
        ProbeObject::Update(dt);
        Destroy();
    }
};

/// 最初の Update で別オブジェクトを Spawn する Component。
class SpawnerComponent : public Component {
public:
    WITCH_COMPONENT(SpawnerComponent, Component)
    void Update(float) override {
        if (spawned_ == nullptr) {
            spawned_ = Owner()->GetScene()->Spawn<ProbeObject>();
        }
    }
    ProbeObject* Spawned() const { return spawned_; }

private:
    ProbeObject* spawned_ = nullptr;
};

TEST_CASE("Spawn is deferred until the next Update", "[Scene]") {
    Scene scene;
    auto* obj = scene.Spawn<ProbeObject>();

    REQUIRE(obj != nullptr);
    REQUIRE(obj->Id() != kInvalidId);
    // 保留リストにいる間は Find で見つからない（RefactoringNotes §8 の文書化された挙動）。
    CHECK(scene.Find(obj->Id()) == nullptr);
    CHECK(obj->spawnCount == 0);

    scene.Update(0.016f);

    // 生成反映は Update の先頭で行われ、同一フレームの Update フェーズにも参加する。
    CHECK(scene.Find(obj->Id()) == obj);
    CHECK(obj->spawnCount == 1);
    CHECK(obj->updateCount == 1);
}

TEST_CASE("Components added before spawn run in the spawn frame", "[Scene]") {
    Scene scene;
    auto* obj = scene.Spawn<ProbeObject>();
    auto* comp = obj->AddComponent<CountingComponent>();

    scene.Update(0.016f);

    CHECK(comp->updateCount == 1);
}

TEST_CASE("Destroy is deferred to end of frame", "[Scene]") {
    Scene scene;
    auto* obj = scene.Spawn<ProbeObject>();
    scene.Update(0.016f);

    const ObjectId id = obj->Id();
    obj->Destroy();

    // フラグが立つだけで、回収されるまでは Find 可能なまま。
    CHECK(obj->IsDestroyed());
    CHECK(scene.Find(id) == obj);
    CHECK(obj->despawnCount == 0);

    // 破棄回収はフレーム末（Update の第 3 段階）。以降 obj は delete 済みで触れない
    // （OnDespawn の回数は別ケースで検証する）。
    scene.Update(0.016f);
    CHECK(scene.Find(id) == nullptr);
}

TEST_CASE("Destroyed object skips Update hook and its components", "[Scene]") {
    Scene scene;
    int componentUpdateCount = 0;
    auto* obj = scene.Spawn<SelfDestroyObject>();
    obj->AddComponent<ExternalCountingComponent>(&componentUpdateCount);

    // 1 フレーム目: Update フック内で自己 Destroy。フック自体は 1 回走るが、
    // 同一フレームの Update フェーズで所有 Component はスキップされ、フレーム末に回収される。
    scene.Update(0.016f);

    CHECK(componentUpdateCount == 0);

    // 2 フレーム目: 回収済みなのでクラッシュせず、何も起きない。
    scene.Update(0.016f);
    CHECK(componentUpdateCount == 0);
}

TEST_CASE("OnDespawn is called exactly once on deferred destroy", "[Scene]") {
    Scene scene;
    // OnDespawn の回数はオブジェクト外のカウンタに書く（delete 後に読むため）。
    static int sDespawnCount;
    sDespawnCount = 0;

    class DespawnProbe : public GameObject {
    public:
        void OnDespawn() override { ++sDespawnCount; }
    };

    auto* obj = scene.Spawn<DespawnProbe>();
    scene.Update(0.016f);
    obj->Destroy();
    scene.Update(0.016f);
    scene.Update(0.016f);

    CHECK(sDespawnCount == 1);
}

TEST_CASE("Spawn during Update is reflected next frame", "[Scene]") {
    Scene scene;
    auto* obj = scene.Spawn<ProbeObject>();
    auto* spawner = obj->AddComponent<SpawnerComponent>();

    scene.Update(0.016f);  // obj 反映 + SpawnerComponent が Spawn を発行

    auto* spawned = spawner->Spawned();
    REQUIRE(spawned != nullptr);
    // 更新中の Spawn は保留され、このフレームでは未反映。
    CHECK(scene.Find(spawned->Id()) == nullptr);
    CHECK(spawned->spawnCount == 0);

    scene.Update(0.016f);  // 次フレーム頭で反映

    CHECK(scene.Find(spawned->Id()) == spawned);
    CHECK(spawned->spawnCount == 1);
}

}  // namespace
