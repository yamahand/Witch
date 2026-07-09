// Scene の 3 段階更新（生成反映 → フェーズ実行 → 破棄回収）の契約テスト。
// 仕様は Scene.h / GameObject.h のコメントに文書化されたもの（順序厳守・遅延 Spawn・遅延破棄）。
// 固定タイムステップ導入後は 1 フレーム = FixedUpdate × 0〜N 回 + FrameUpdate × 1 回。
// 通常フレームの契約は StepFrame（Fixed + Frame 各 1 回）で、固定/毎フレーム分割固有の
// 契約（0 ステップ・多重ステップ）は末尾の専用ケースで検証する。
#include "WitchEngine/Scene/Component.h"
#include "WitchEngine/Scene/GameObject.h"
#include "WitchEngine/Scene/Scene.h"

#include <catch2/catch_test_macros.hpp>

namespace {

using namespace witch;

/// 固定ステップの dt。契約上 FixedUpdate には常にこの値を渡す（Time::kFixedDelta 相当）。
constexpr float kFixedDt = 1.0f / 60.0f;

/// 1 フレーム = 固定ステップ 1 回 + フレーム更新 1 回として回すヘルパ。
/// FixedUpdate の dt は契約どおり固定値、FrameUpdate の dt だけ可変にできる。
void StepFrame(Scene& scene, float frameDt = kFixedDt) {
    scene.FixedUpdate(kFixedDt);
    scene.FrameUpdate(frameDt);
}

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

/// Update 回数を数えるだけの Component（既定フェーズ = Update = 固定ステップ側）。
class CountingComponent : public Component {
public:
    WITCH_COMPONENT(CountingComponent, Component)
    void Update(float) override { ++updateCount; }
    int updateCount = 0;
};

/// Render フェーズ（毎フレーム側）で Update 回数を数える Component。
class RenderCountingComponent : public Component {
public:
    WITCH_COMPONENT(RenderCountingComponent, Component)
    UpdatePhase Phase() const override { return UpdatePhase::Render; }
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

/// ExternalCountingComponent の Render フェーズ（毎フレーム側）版。
class ExternalRenderCountingComponent : public Component {
public:
    WITCH_COMPONENT(ExternalRenderCountingComponent, Component)
    explicit ExternalRenderCountingComponent(int* counter) : counter_(counter) {}
    UpdatePhase Phase() const override { return UpdatePhase::Render; }
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

TEST_CASE("Spawn is deferred until the next update", "[Scene]") {
    Scene scene;
    auto* obj = scene.Spawn<ProbeObject>();

    REQUIRE(obj != nullptr);
    REQUIRE(obj->Id() != kInvalidId);
    // 保留リストにいる間は Find で見つからない（RefactoringNotes §8 の文書化された挙動）。
    CHECK(scene.Find(obj->Id()) == nullptr);
    CHECK(obj->spawnCount == 0);

    StepFrame(scene);

    // 生成反映は FixedUpdate の先頭で行われ、同一ステップの Update フェーズにも参加する。
    CHECK(scene.Find(obj->Id()) == obj);
    CHECK(obj->spawnCount == 1);
    CHECK(obj->updateCount == 1);
}

TEST_CASE("Components added before spawn run in the spawn frame", "[Scene]") {
    Scene scene;
    auto* obj = scene.Spawn<ProbeObject>();
    auto* comp = obj->AddComponent<CountingComponent>();

    StepFrame(scene);

    CHECK(comp->updateCount == 1);
}

TEST_CASE("Destroy is deferred to end of frame", "[Scene]") {
    Scene scene;
    auto* obj = scene.Spawn<ProbeObject>();
    StepFrame(scene);

    const ObjectId id = obj->Id();
    obj->Destroy();

    // フラグが立つだけで、回収されるまでは Find 可能なまま。
    CHECK(obj->IsDestroyed());
    CHECK(scene.Find(id) == obj);
    CHECK(obj->despawnCount == 0);

    // 破棄回収はフレーム末（FrameUpdate の末尾）。以降 obj は delete 済みで触れない
    // （OnDespawn の回数は別ケースで検証する）。
    StepFrame(scene);
    CHECK(scene.Find(id) == nullptr);
}

TEST_CASE("Destroyed object skips Update hook and its components", "[Scene]") {
    Scene scene;
    int componentUpdateCount = 0;
    auto* obj = scene.Spawn<SelfDestroyObject>();
    obj->AddComponent<ExternalCountingComponent>(&componentUpdateCount);

    // 1 フレーム目: Update フック内で自己 Destroy。フック自体は 1 回走るが、
    // 同一ステップの Update フェーズで所有 Component はスキップされ、フレーム末に回収される。
    StepFrame(scene);

    CHECK(componentUpdateCount == 0);

    // 2 フレーム目: 回収済みなのでクラッシュせず、何も起きない。
    StepFrame(scene);
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
    StepFrame(scene);
    obj->Destroy();
    StepFrame(scene);
    StepFrame(scene);

    CHECK(sDespawnCount == 1);
}

TEST_CASE("Spawn during update is deferred to the next update stage", "[Scene]") {
    Scene scene;
    auto* obj = scene.Spawn<ProbeObject>();
    auto* spawner = obj->AddComponent<SpawnerComponent>();

    scene.FixedUpdate(kFixedDt);  // obj 反映 + SpawnerComponent が Spawn を発行

    auto* spawned = spawner->Spawned();
    REQUIRE(spawned != nullptr);
    // 更新中の Spawn は保留され、このステップでは未反映。
    CHECK(scene.Find(spawned->Id()) == nullptr);
    CHECK(spawned->spawnCount == 0);

    scene.FrameUpdate(kFixedDt);  // 同フレームの FrameUpdate 先頭で反映（描画に参加できる）

    CHECK(scene.Find(spawned->Id()) == spawned);
    CHECK(spawned->spawnCount == 1);
}

// ── 固定 / 毎フレーム分割固有の契約 ─────────────────────────────────────────

TEST_CASE("Zero-step frame still spawns and renders but skips fixed phases", "[Scene]") {
    // 高リフレッシュレート環境では固定ステップが 1 回も走らないフレームがある。
    // その場合も Spawn は反映され、Render フェーズ（毎フレーム側）は走り、
    // Update フェーズ（固定側）は走らない = 「画面が空にならない」保証。
    Scene scene;
    auto* obj = scene.Spawn<ProbeObject>();
    auto* logic = obj->AddComponent<CountingComponent>();
    auto* render = obj->AddComponent<RenderCountingComponent>();

    scene.FrameUpdate(0.008f);  // FixedUpdate なし

    CHECK(obj->spawnCount == 1);
    CHECK(scene.Find(obj->Id()) == obj);
    CHECK(render->updateCount == 1);
    CHECK(logic->updateCount == 0);
    CHECK(obj->updateCount == 0);  // GameObject::Update フックも固定側
}

TEST_CASE("Multi-step frame runs fixed phases per step and render once", "[Scene]") {
    Scene scene;
    auto* obj = scene.Spawn<ProbeObject>();
    auto* logic = obj->AddComponent<CountingComponent>();
    auto* render = obj->AddComponent<RenderCountingComponent>();

    // キャッチアップフレーム: 固定ステップ 3 回 + フレーム更新 1 回。
    scene.FixedUpdate(kFixedDt);
    scene.FixedUpdate(kFixedDt);
    scene.FixedUpdate(kFixedDt);
    scene.FrameUpdate(0.05f);

    CHECK(logic->updateCount == 3);
    CHECK(obj->updateCount == 3);
    CHECK(render->updateCount == 1);  // 描画提出は多重にならない
}

TEST_CASE("Spawn during a fixed step joins the same frame's render", "[Scene]") {
    Scene scene;
    auto* obj = scene.Spawn<ProbeObject>();
    auto* spawner = obj->AddComponent<SpawnerComponent>();

    scene.FixedUpdate(kFixedDt);  // spawner が Spawn（保留）
    auto* spawned = spawner->Spawned();
    REQUIRE(spawned != nullptr);
    auto* render = spawned->AddComponent<RenderCountingComponent>();

    scene.FrameUpdate(kFixedDt);

    // 同一フレームの FrameUpdate 先頭で反映され、そのフレームの描画に乗る。
    CHECK(spawned->spawnCount == 1);
    CHECK(render->updateCount == 1);
    // 固定側の Update はまだ走っていない（次の FixedUpdate から）。
    CHECK(spawned->updateCount == 0);

    scene.FixedUpdate(kFixedDt);
    CHECK(spawned->updateCount == 1);
}

TEST_CASE("Destroy during a fixed step skips remaining phases and despawns once",
          "[Scene]") {
    Scene scene;
    static int sDespawnCount;
    sDespawnCount = 0;

    class DespawnCountingSelfDestroy : public SelfDestroyObject {
    public:
        void OnDespawn() override { ++sDespawnCount; }
    };

    int renderCount = 0;
    auto* obj = scene.Spawn<DespawnCountingSelfDestroy>();
    obj->AddComponent<ExternalRenderCountingComponent>(&renderCount);
    StepFrame(scene);  // 反映 → FixedUpdate の Update フックで自己 Destroy → フレーム末回収

    // 自己 Destroy は FixedUpdate 中に起きているため、同フレームの Render は
    // スキップされ（描画に乗らず）、FrameUpdate 末尾で回収済み。
    CHECK(renderCount == 0);
    CHECK(sDespawnCount == 1);

    // 以降のフレームでは何も起きない（二重 OnDespawn なし）。
    StepFrame(scene);
    CHECK(sDespawnCount == 1);
}

}  // namespace
