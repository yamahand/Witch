// CollisionComponent の契約テスト: Physics フェーズ（固定側）での速度自動積分、
// Update フェーズで書いた速度の同一ステップ反映、タイル押し戻し（Collision.ldtk
// フィクスチャ = 外周 1 セルが壁の 8x6 セル箱部屋、gridSize 8px、内側 x:8-56 y:8-40）。
#include "WitchEngine/Physics2D/CollisionComponent.h"
#include "WitchEngine/Scene/GameObject.h"
#include "WitchEngine/Scene/Scene.h"
#include "TestHelpers.h"

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace {

using namespace witch;
using namespace witch::test;
using Catch::Approx;

/// Update フェーズで一度だけ速度を書き込むコントローラ役の Component。
/// Update（ロジック）→ Physics（積分）が同一固定ステップ内で起きる契約の検証に使う。
class VelocityWriterComponent : public Component {
public:
    WITCH_COMPONENT(VelocityWriterComponent, Component)
    void Update(float) override {
        if (!written_) {
            written_ = true;
            Owner()->GetComponent<CollisionComponent>()->SetVelocity(vx, vy);
        }
    }
    float vx = 0.0f;
    float vy = 0.0f;

private:
    bool written_ = false;
};

/// レベル内の指定位置に CollisionComponent 付きで現れるテスト用オブジェクト。
class BodyObject : public GameObject {
public:
    BodyObject(float x, float y, float w = 6.0f, float h = 6.0f)
        : w_(w), h_(h) {
        transform.x = x;
        transform.y = y;
    }
    void OnSpawn() override { collision = AddComponent<CollisionComponent>(w_, h_); }
    CollisionComponent* collision = nullptr;

private:
    float w_, h_;
};

} // namespace

TEST_CASE("CollisionComponent integrates velocity without a level", "[Collision]") {
    Scene scene;
    auto* body = scene.Spawn<BodyObject>(10.0f, 20.0f);
    StepFrame(scene);  // 生成反映
    body->collision->SetVelocity(60.0f, -30.0f);

    StepFrame(scene);  // 1 固定ステップ = 1/60 秒
    CHECK(body->transform.x == Approx(10.0f + 1.0f));
    CHECK(body->transform.y == Approx(20.0f - 0.5f));
    CHECK_FALSE(body->collision->OnGround());
    CHECK_FALSE(body->collision->HitWall());
}

TEST_CASE("Velocity written in Update phase moves the body in the same fixed step",
          "[Collision]") {
    Scene scene;
    auto* body = scene.Spawn<BodyObject>(0.0f, 0.0f);
    StepFrame(scene);
    auto* writer = body->AddComponent<VelocityWriterComponent>();
    writer->vx = 60.0f;

    // 同一 FixedUpdate 内で Update フェーズ（速度書き込み）→ Physics フェーズ（積分）
    // の順に走るため、この 1 ステップで既に移動している。
    StepFrame(scene);
    CHECK(body->transform.x == Approx(1.0f));
}

TEST_CASE("CollisionComponent lands on the floor and stops falling", "[Collision]") {
    ScopedFixtureVfs vfsGuard;
    Scene scene;
    REQUIRE(scene.LoadLevel("Collision.ldtk").has_value());

    // 部屋中央から落下。床上辺 y = 40、AABB h = 6（中心基準）→ 接地時 transform.y = 37。
    auto* body = scene.Spawn<BodyObject>(32.0f, 24.0f);
    StepFrame(scene);
    body->collision->SetVelocity(0.0f, 300.0f);  // 5 px/step

    for (int i = 0; i < 10; ++i) {
        StepFrame(scene);
        body->collision->SetVelocityY(300.0f);  // 重力役: 毎ステップ下向き速度を与える
    }
    CHECK(body->transform.y == Approx(37.0f));
    CHECK(body->collision->OnGround());
    CHECK_FALSE(body->collision->HitHead());
}

TEST_CASE("CollisionComponent zeroes the blocked velocity axis on impact",
          "[Collision]") {
    ScopedFixtureVfs vfsGuard;
    Scene scene;
    REQUIRE(scene.LoadLevel("Collision.ldtk").has_value());

    auto* body = scene.Spawn<BodyObject>(32.0f, 24.0f);
    StepFrame(scene);
    body->collision->SetVelocity(0.0f, 600.0f);  // 10 px/step → 2 ステップ目で着地

    StepFrame(scene);
    StepFrame(scene);
    CHECK(body->collision->OnGround());
    CHECK(body->collision->VelocityY() == 0.0f);  // 着地で落下停止
}

TEST_CASE("CollisionComponent stops at walls and the ceiling", "[Collision]") {
    ScopedFixtureVfs vfsGuard;
    Scene scene;
    REQUIRE(scene.LoadLevel("Collision.ldtk").has_value());

    SECTION("右壁: 内側右端 x = 56、AABB w = 6 → transform.x = 53 で停止") {
        auto* body = scene.Spawn<BodyObject>(32.0f, 24.0f);
        StepFrame(scene);
        for (int i = 0; i < 10; ++i) {
            body->collision->SetVelocityX(600.0f);
            StepFrame(scene);
        }
        CHECK(body->transform.x == Approx(53.0f));
        CHECK(body->collision->HitRight());
        CHECK(body->collision->HitWall());
    }
    SECTION("天井: 内側上端 y = 8 → transform.y = 11 で停止 + HitHead") {
        auto* body = scene.Spawn<BodyObject>(32.0f, 24.0f);
        StepFrame(scene);
        for (int i = 0; i < 10; ++i) {
            body->collision->SetVelocityY(-600.0f);
            StepFrame(scene);
        }
        CHECK(body->transform.y == Approx(11.0f));
        CHECK(body->collision->HitHead());
    }
}

TEST_CASE("SetSolidVsTiles(false) passes through tiles but still integrates",
          "[Collision]") {
    ScopedFixtureVfs vfsGuard;
    Scene scene;
    REQUIRE(scene.LoadLevel("Collision.ldtk").has_value());

    auto* body = scene.Spawn<BodyObject>(32.0f, 24.0f);
    StepFrame(scene);
    body->collision->SetSolidVsTiles(false);
    for (int i = 0; i < 20; ++i) {
        body->collision->SetVelocityY(600.0f);
        StepFrame(scene);
    }
    CHECK(body->transform.y > 48.0f);  // 床（レベル外まで）すり抜け
    CHECK_FALSE(body->collision->OnGround());
}

// ── 45° 坂の統合（CollisionSlope.ldtk = 地面 y=32、'/' 坂 x32..40、高台 y=24） ──

TEST_CASE("Body walks up the ramp onto the plateau and back down a ledge, "
          "staying grounded",
          "[Collision][Slope]") {
    ScopedFixtureVfs vfsGuard;
    Scene scene;
    REQUIRE(scene.LoadLevel("CollisionSlope.ldtk").has_value());

    // 地面（下端 32 → transform.y = 29）から右へ 1px/ステップで歩く。
    // 重力役として毎ステップ下向き速度を与える（コントローラと同じ使い方）。
    auto* body = scene.Spawn<BodyObject>(20.0f, 29.0f);
    StepFrame(scene);  // 生成反映
    body->collision->SetVelocityY(300.0f);  // 重力相当を与えて着地させる
    StepFrame(scene);
    REQUIRE(body->collision->OnGround());

    bool allGrounded = true;
    float minY = body->transform.y;
    for (int i = 0; i < 36; ++i) {
        body->collision->SetVelocity(60.0f, 300.0f);  // 1px/step 右 + 重力相当
        StepFrame(scene);
        allGrounded = allGrounded && body->collision->OnGround();
        minY = std::min(minY, body->transform.y);
    }
    // 坂を登り（y が下がる = 高くなる）、高台（下端 24 → y=21）を経て
    // 右の段差（8px）を吸着で降り、地面（y=29）へ戻って右壁で止まる。
    // 全ステップ接地維持（登坂・乗り移り・段差降りのどこでも浮かない）。
    CHECK(allGrounded);
    CHECK(minY == Catch::Approx(21.0f));  // 高台の上に居た瞬間がある
    CHECK(body->collision->HitRight());   // 右壁（x=56）到達
    CHECK(body->transform.x == Catch::Approx(53.0f));
    CHECK(body->transform.y == Catch::Approx(29.0f));
}

TEST_CASE("Jump from a slope is not cancelled by ground snapping",
          "[Collision][Slope]") {
    ScopedFixtureVfs vfsGuard;
    Scene scene;
    REQUIRE(scene.LoadLevel("CollisionSlope.ldtk").has_value());

    // 坂の中腹（中心 36、表面 28 → y=25）に立たせる。
    auto* body = scene.Spawn<BodyObject>(36.0f, 25.0f);
    StepFrame(scene);
    body->collision->SetVelocityY(300.0f);
    StepFrame(scene);
    REQUIRE(body->collision->OnGround());

    // 上向き速度（ジャンプ）を与えたステップでは吸着されず離陸する。
    body->collision->SetVelocityY(-300.0f);  // -5px/step
    StepFrame(scene);
    CHECK_FALSE(body->collision->OnGround());
    CHECK(body->transform.y < 25.0f - 4.0f);  // 上昇している
}

// ── エンティティ同士の重なり（CollisionWorld） ──────────────────────────────

namespace {

/// PostUpdate フェーズで兄弟 CollisionComponent の接触数を記録する観測用 Component。
/// 「PostUpdate から同一ステップの接触が読める」契約の検証に使う。
class ContactProbeComponent : public Component {
public:
    WITCH_COMPONENT(ContactProbeComponent, Component)
    UpdatePhase Phase() const override { return UpdatePhase::PostUpdate; }
    void Update(float) override {
        lastContactCount =
            static_cast<int>(Owner()->GetComponent<CollisionComponent>()->Contacts().size());
    }
    int lastContactCount = -1;
};

} // namespace

TEST_CASE("Overlapping bodies appear in each other's contact list", "[Collision]") {
    Scene scene;
    auto* a = scene.Spawn<BodyObject>(10.0f, 10.0f);
    auto* b = scene.Spawn<BodyObject>(12.0f, 10.0f);  // AABB [9..15] と [7..13] が重なる
    StepFrame(scene);
    StepFrame(scene);  // 初回 Update で遅延登録 → この検出から接触が載る

    REQUIRE(a->collision->Contacts().size() == 1);
    REQUIRE(b->collision->Contacts().size() == 1);
    CHECK(a->collision->Contacts()[0].otherId == b->Id());
    CHECK(a->collision->Contacts()[0].other == b->collision);
    CHECK(b->collision->Contacts()[0].otherId == a->Id());
}

TEST_CASE("Layer/mask filtering is asymmetric", "[Collision]") {
    Scene scene;
    auto* bullet = scene.Spawn<BodyObject>(10.0f, 10.0f);
    auto* enemy = scene.Spawn<BodyObject>(12.0f, 10.0f);
    StepFrame(scene);

    // 弾は敵に当たりたい（mask に敵ビット）が、敵は弾を無視する（mask 0）。
    constexpr uint32_t kEnemyLayer = 1u << 1;
    bullet->collision->SetLayer(1u << 2);
    bullet->collision->SetMask(kEnemyLayer);
    enemy->collision->SetLayer(kEnemyLayer);
    enemy->collision->SetMask(0u);

    StepFrame(scene);
    CHECK(bullet->collision->Contacts().size() == 1);
    CHECK(enemy->collision->Contacts().empty());
}

TEST_CASE("Contacts disappear on the step after separation", "[Collision]") {
    Scene scene;
    auto* a = scene.Spawn<BodyObject>(10.0f, 10.0f);
    auto* b = scene.Spawn<BodyObject>(12.0f, 10.0f);
    StepFrame(scene);
    StepFrame(scene);
    REQUIRE(a->collision->Contacts().size() == 1);

    b->transform.x = 100.0f;  // 引き離す
    StepFrame(scene);
    CHECK(a->collision->Contacts().empty());
    CHECK(b->collision->Contacts().empty());
}

TEST_CASE("Overlap callbacks fire after detection has fully completed", "[Collision]") {
    Scene scene;
    auto* a = scene.Spawn<BodyObject>(10.0f, 10.0f);
    auto* b = scene.Spawn<BodyObject>(12.0f, 10.0f);
    StepFrame(scene);

    // A のコールバック内から相手（B）の接触リストを読む。検出パスが完了してから
    // dispatch される契約なので、B 側にも既に A が載っているはず。
    int callbackCount = 0;
    bool partnerSawMe = false;
    a->collision->SetOverlapCallback([&](const CollisionContact& contact) {
        ++callbackCount;
        for (const CollisionContact& c : contact.other->Contacts()) {
            if (c.otherId == a->Id()) {
                partnerSawMe = true;
            }
        }
    });

    StepFrame(scene);
    CHECK(callbackCount == 1);
    CHECK(partnerSawMe);
    REQUIRE(a->collision->Contacts().size() == 1);
    CHECK(a->collision->Contacts()[0].otherId == b->Id());
}

TEST_CASE("Destroy inside an overlap callback is safe and both sides still fire",
          "[Collision]") {
    Scene scene;
    auto* bullet = scene.Spawn<BodyObject>(10.0f, 10.0f);
    auto* enemy = scene.Spawn<BodyObject>(12.0f, 10.0f);
    StepFrame(scene);

    // 弾はヒットで自壊。敵側の被弾コールバックも同ステップで発火する
    // （dispatch は破棄フラグをスキップしない契約）。
    int bulletHits = 0;
    int enemyHits = 0;
    bullet->collision->SetOverlapCallback([&](const CollisionContact&) {
        ++bulletHits;
        bullet->Destroy();
    });
    enemy->collision->SetOverlapCallback([&](const CollisionContact&) { ++enemyHits; });

    StepFrame(scene);
    CHECK(bulletHits == 1);
    CHECK(enemyHits == 1);

    // 弾はフレーム末で回収済み。以降の検出でクラッシュせず、接触も発生しない。
    const ObjectId bulletId = bullet->Id();
    StepFrame(scene);
    CHECK(scene.Find(bulletId) == nullptr);
    CHECK(enemy->collision->Contacts().empty());
    CHECK(enemyHits == 1);
}

TEST_CASE("PostUpdate phase reads contacts from the same fixed step", "[Collision]") {
    Scene scene;
    auto* a = scene.Spawn<BodyObject>(10.0f, 10.0f);
    auto* b = scene.Spawn<BodyObject>(12.0f, 10.0f);
    StepFrame(scene);
    auto* probe = a->AddComponent<ContactProbeComponent>();

    StepFrame(scene);
    CHECK(probe->lastContactCount == 1);

    b->transform.x = 100.0f;
    StepFrame(scene);
    CHECK(probe->lastContactCount == 0);
}

TEST_CASE("Colliders on the same GameObject do not contact each other", "[Collision]") {
    Scene scene;
    auto* body = scene.Spawn<BodyObject>(10.0f, 10.0f);
    StepFrame(scene);
    auto* second = body->AddComponent<CollisionComponent>(4.0f, 4.0f);

    StepFrame(scene);
    StepFrame(scene);
    CHECK(body->collision->Contacts().empty());
    CHECK(second->Contacts().empty());
}

TEST_CASE("CollisionComponent re-resolves the grid after LoadLevel reload",
          "[Collision]") {
    ScopedFixtureVfs vfsGuard;
    Scene scene;
    REQUIRE(scene.LoadLevel("Collision.ldtk").has_value());

    auto* body = scene.Spawn<BodyObject>(32.0f, 24.0f);
    StepFrame(scene);
    body->collision->SetVelocityY(600.0f);
    StepFrame(scene);  // 旧レベルの IntGrid をキャッシュさせる

    // 再ロードで LevelData が差し替わる（旧ポインタは破棄される）。
    // キャッシュがポインタ比較で引き直され、dangling を踏まずに動き続けること。
    REQUIRE(scene.LoadLevel("Collision.ldtk").has_value());
    for (int i = 0; i < 10; ++i) {
        body->collision->SetVelocityY(600.0f);
        StepFrame(scene);
    }
    CHECK(body->transform.y == Approx(37.0f));  // 新レベルの床で接地
    CHECK(body->collision->OnGround());
}
