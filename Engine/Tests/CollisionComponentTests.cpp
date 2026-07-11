// CollisionComponent の契約テスト: Physics フェーズ（固定側）での速度自動積分、
// Update フェーズで書いた速度の同一ステップ反映、タイル押し戻し（Collision.ldtk
// フィクスチャ = 外周 1 セルが壁の 8x6 セル箱部屋、gridSize 8px、内側 x:8-56 y:8-40）。
#include "WitchEngine/Physics2D/CollisionComponent.h"
#include "WitchEngine/Scene/GameObject.h"
#include "WitchEngine/Scene/Scene.h"
#include "TestHelpers.h"

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
