// Scene の 3 段階更新（生成反映 → フェーズ実行 → 破棄回収）の契約テスト。
// 仕様は Scene.h / GameObject.h のコメントに文書化されたもの（順序厳守・遅延 Spawn・遅延破棄）。
// 固定タイムステップ導入後は 1 フレーム = FixedUpdate × 0〜N 回 + FrameUpdate × 1 回。
// 通常フレームの契約は StepFrame（Fixed + Frame 各 1 回）で、固定/毎フレーム分割固有の
// 契約（0 ステップ・多重ステップ）は末尾の専用ケースで検証する。
#include "WitchEngine/Core/ObjectRegistry.h"
#include "WitchEngine/Core/Services.h"
#include "WitchEngine/Scene/Component.h"
#include "WitchEngine/Scene/GameObject.h"
#include "WitchEngine/Scene/Scene.h"
#include "WitchEngine/Vfs/Vfs.h"
#include "TestHelpers.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace {

using namespace witch;
using namespace witch::test;

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
    // OnEnter 外の Spawn は保留リストに積まれ、反映まで Find で見つからない（Scene.h の契約）。
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

// ── OnEnter 中の即時 Spawn（Enter() 経由の即時反映モード） ──────────────────

/// コンストラクタ引数で位置を受け、OnSpawn がその値を観測する GameObject。
/// 「OnSpawn が必要とする情報はコンストラクタ引数で渡す」自己完結契約の検証用。
class ConfiguredObject : public GameObject {
public:
    explicit ConfiguredObject(float x) { transform.x = x; }
    void OnSpawn() override { xSeenByOnSpawn = transform.x; }
    float xSeenByOnSpawn = -1.0f;
};

/// OnSpawn でさらに Spawn する GameObject（入れ子 Spawn の即時性検証用）。
class NestedSpawnObject : public GameObject {
public:
    void OnSpawn() override { child = GetScene()->Spawn<ProbeObject>(); }
    ProbeObject* child = nullptr;
};

TEST_CASE("Spawn during OnEnter is applied immediately", "[Scene]") {
    class EnterScene : public Scene {
    public:
        ProbeObject* spawned = nullptr;
        bool foundDuringEnter = false;
        int spawnCountDuringEnter = -1;

    protected:
        void OnEnter() override {
            spawned = Spawn<ProbeObject>();
            // Spawn の戻り時点で反映済み: Find が通り、OnSpawn も完了している。
            foundDuringEnter = (Find(spawned->Id()) == spawned);
            spawnCountDuringEnter = spawned->spawnCount;
        }
    };

    EnterScene scene;
    scene.Enter();

    CHECK(scene.foundDuringEnter);
    CHECK(scene.spawnCountDuringEnter == 1);
    // 即時なのは反映だけで、更新はまだ走らない（最初の FixedUpdate から）。
    CHECK(scene.spawned->updateCount == 0);

    // Enter 後の通常フレームでは二重反映されず、普通に更新に参加する。
    StepFrame(scene);
    CHECK(scene.spawned->spawnCount == 1);
    CHECK(scene.spawned->updateCount == 1);
}

TEST_CASE("OnSpawn sees constructor-arg state in both spawn modes", "[Scene]") {
    // 即時モード（OnEnter 中）でも遅延モードでも、OnSpawn が見る状態は
    // コンストラクタ引数由来で同一になる（自己完結契約が成立していること）。
    class EnterScene : public Scene {
    public:
        ConfiguredObject* obj = nullptr;

    protected:
        void OnEnter() override { obj = Spawn<ConfiguredObject>(100.0f); }
    };

    EnterScene immediate;
    immediate.Enter();
    CHECK(immediate.obj->xSeenByOnSpawn == 100.0f);

    Scene deferred;
    auto* obj = deferred.Spawn<ConfiguredObject>(100.0f);
    StepFrame(deferred);
    CHECK(obj->xSeenByOnSpawn == 100.0f);
}

TEST_CASE("Nested Spawn inside OnSpawn during OnEnter is also immediate", "[Scene]") {
    class EnterScene : public Scene {
    public:
        NestedSpawnObject* parent = nullptr;

    protected:
        void OnEnter() override { parent = Spawn<NestedSpawnObject>(); }
    };

    EnterScene scene;
    scene.Enter();

    REQUIRE(scene.parent->child != nullptr);
    CHECK(scene.Find(scene.parent->child->Id()) == scene.parent->child);
    CHECK(scene.parent->child->spawnCount == 1);
}

TEST_CASE("Components added after an immediate spawn join updates", "[Scene]") {
    // 即時反映後の AddComponent は RegisterComponent の個別登録経路を通る。
    // その経路でもスケジューラに載り、以降のフレームで普通に更新されること。
    class EnterScene : public Scene {
    public:
        CountingComponent* comp = nullptr;
        RenderCountingComponent* render = nullptr;

    protected:
        void OnEnter() override {
            auto* obj = Spawn<ProbeObject>();
            comp = obj->AddComponent<CountingComponent>();
            render = obj->AddComponent<RenderCountingComponent>();
        }
    };

    EnterScene scene;
    scene.Enter();
    StepFrame(scene);

    CHECK(scene.comp->updateCount == 1);
    CHECK(scene.render->updateCount == 1);
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

// ── LoadLevel（ObjectRegistry 経由の実体化 = AdoptSpawn 経路） ──────────────────

/// TestLevelEntity::OnSpawn が観測した状態のキャプチャ。
/// LoadLevel は transform / Name を AdoptSpawn（= OnSpawn）より前に設定する契約
/// なので、OnSpawn 時点の値を検証できる。
struct LevelEntitySpawnCapture {
    int spawnCount = 0;
    float x = 0.0f, y = 0.0f;
    std::string name;
    bool sceneSet = false;
    ObjectId id = 0;
};
LevelEntitySpawnCapture sLevelEntityCapture;

/// レベルファイルから ObjectRegistry 経由で実体化されるダミーエンティティ。
class TestLevelEntity : public GameObject {
public:
    void OnSpawn() override {
        ++sLevelEntityCapture.spawnCount;
        sLevelEntityCapture.x = transform.x;
        sLevelEntityCapture.y = transform.y;
        sLevelEntityCapture.name = Name();
        sLevelEntityCapture.sceneSet = GetScene() != nullptr;
        sLevelEntityCapture.id = Id();
    }
};
WITCH_REGISTER_OBJECT(TestLevelEntity);

/// Engine/Tests/Fixtures/ をマウントした VFS を Services に差し込み、
/// スコープ終了時に元へ戻す（他テストへの影響を残さない）。
struct ScopedFixtureVfs {
    vfs::Vfs vfs;
    vfs::Vfs* prev;
    ScopedFixtureVfs() {
        REQUIRE(vfs.MountDisk(WITCH_TEST_FIXTURE_DIR).has_value());
        vfs.Seal();
        prev = Services::Instance().vfs;
        Services::Instance().vfs = &vfs;
    }
    ~ScopedFixtureVfs() { Services::Instance().vfs = prev; }
};

TEST_CASE("LoadLevel spawns registered entities immediately during OnEnter", "[Scene]") {
    ScopedFixtureVfs vfsGuard;
    sLevelEntityCapture = {};

    class LoadingScene : public Scene {
    public:
        std::expected<void, std::string> result;

    protected:
        void OnEnter() override { result = LoadLevel("Entities.ldtk"); }
    };

    LoadingScene scene;
    scene.Enter();
    REQUIRE(scene.result.has_value());

    // 登録済みエンティティは 1 体だけ実体化され、未登録名（UnregisteredEntity）は
    // クラッシュせずスキップされる。
    CHECK(sLevelEntityCapture.spawnCount == 1);
    // OnSpawn 時点で transform / Name / シーン参照 / Id が設定済み。
    CHECK(sLevelEntityCapture.x == 24.0f);
    CHECK(sLevelEntityCapture.y == 40.0f);
    CHECK(sLevelEntityCapture.name == "TestLevelEntity");
    CHECK(sLevelEntityCapture.sceneSet);
    CHECK(sLevelEntityCapture.id != 0);
    // OnEnter 中は即時反映なので、Enter 完了時点で Find が通る。
    CHECK(scene.Find(sLevelEntityCapture.id) != nullptr);

    // レベルデータの保持と背景クリア色（#102030）の反映。
    REQUIRE(scene.CurrentLevel() != nullptr);
    CHECK(scene.CurrentLevel()->identifier == "EntityLevel");
    CHECK(scene.ClearColor().r == Catch::Approx(0x10 / 255.0f));
    CHECK(scene.ClearColor().g == Catch::Approx(0x20 / 255.0f));
    CHECK(scene.ClearColor().b == Catch::Approx(0x30 / 255.0f));
}

TEST_CASE("LoadLevel outside OnEnter defers entity spawn to the next update", "[Scene]") {
    ScopedFixtureVfs vfsGuard;
    sLevelEntityCapture = {};

    Scene scene;
    REQUIRE(scene.LoadLevel("Entities.ldtk").has_value());
    CHECK(sLevelEntityCapture.spawnCount == 0);  // 保留リスト止まり

    StepFrame(scene);
    CHECK(sLevelEntityCapture.spawnCount == 1);
    CHECK(scene.Find(sLevelEntityCapture.id) != nullptr);
}

TEST_CASE("LoadLevel destroys the previous level's objects on reload", "[Scene]") {
    ScopedFixtureVfs vfsGuard;
    sLevelEntityCapture = {};

    Scene scene;
    REQUIRE(scene.LoadLevel("Entities.ldtk").has_value());
    StepFrame(scene);
    const ObjectId first = sLevelEntityCapture.id;
    REQUIRE(scene.Find(first) != nullptr);

    // 再ロード: 旧レベル由来のオブジェクトは Destroy され、フレーム末で回収される。
    REQUIRE(scene.LoadLevel("Entities.ldtk").has_value());
    StepFrame(scene);
    CHECK(scene.Find(first) == nullptr);
    CHECK(sLevelEntityCapture.id != first);
    CHECK(scene.Find(sLevelEntityCapture.id) != nullptr);
}

TEST_CASE("LoadLevel reload also destroys still-pending objects", "[Scene]") {
    ScopedFixtureVfs vfsGuard;
    sLevelEntityCapture = {};

    // 更新を挟まず 2 連続でロード: 1 回目のエンティティは pendingSpawn_ に
    // いるうちに Destroy され、反映後のフレーム末で回収される。
    Scene scene;
    REQUIRE(scene.LoadLevel("Entities.ldtk").has_value());
    REQUIRE(scene.LoadLevel("Entities.ldtk").has_value());
    StepFrame(scene);
    // 生き残るのは 2 回目のエンティティ 1 体だけ。
    CHECK(scene.Find(sLevelEntityCapture.id) != nullptr);
    CHECK(sLevelEntityCapture.spawnCount == 2);  // 両方 OnSpawn は通る（遅延破棄契約）
}

TEST_CASE("LoadLevel reports missing files as errors", "[Scene]") {
    ScopedFixtureVfs vfsGuard;
    Scene scene;
    CHECK_FALSE(scene.LoadLevel("DoesNotExist.ldtk").has_value());
    CHECK(scene.CurrentLevel() == nullptr);
}

}  // namespace
