#include "WitchEngine/Physics2D/TileCollision.h"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <initializer_list>
#include <string_view>

using witch::Aabb;
using witch::LevelData;
using witch::LevelIntGrid;
using namespace witch::physics2d;

namespace {

constexpr int kGs = 8;  // テスト用グリッドの 1 セル px

/// 文字アートから IntGrid を作る（'#' = ソリッド、'/' = 右上がり坂、
/// '\' = 左上がり坂、'.' = 空）。行優先・y-down。
LevelIntGrid MakeGrid(std::initializer_list<std::string_view> rows) {
    LevelIntGrid grid;
    grid.gridSize = kGs;
    grid.height = static_cast<int>(rows.size());
    for (std::string_view row : rows) {
        grid.width = static_cast<int>(row.size());
        for (char c : row) {
            int value = 0;
            if (c == '#') value = 1;
            if (c == '/') value = 2;
            if (c == '\\') value = 3;
            grid.values.push_back(value);
        }
    }
    return grid;
}

/// 登坂・頂上乗り移りテスト用のランプ地形。
/// 地面 = row 3（上辺 y = 24）、'/' 坂 = (4,2)（x 32..40、表面 24→16）、
/// 高台 = (5,2)（上辺 y = 16）。
LevelIntGrid MakeRampGrid() {
    return MakeGrid({
        "......",
        "......",
        "..../#",
        "######",
    });
}

} // namespace

TEST_CASE("ShapeFromValue interprets cell values (0=empty, 2/3=slopes, else solid)",
          "[TileCollision]") {
    CHECK(ShapeFromValue(0) == TileShape::Empty);
    CHECK(ShapeFromValue(1) == TileShape::Solid);
    CHECK(ShapeFromValue(2) == TileShape::SlopeUpRight);
    CHECK(ShapeFromValue(3) == TileShape::SlopeUpLeft);
    CHECK(ShapeFromValue(7) == TileShape::Solid);
}

TEST_CASE("MoveAabb lands on floor and snaps to tile boundary", "[TileCollision]") {
    // 床 = 最下段（row 4、上辺 y = 32）
    const LevelIntGrid grid = MakeGrid({
        "......",
        "......",
        "......",
        "......",
        "######",
    });
    const Aabb box{9.0f, 10.0f, 6.0f, 6.0f};

    const MoveResult r = MoveAabb(grid, box, 0.0f, 30.0f);
    CHECK(r.x == 9.0f);
    CHECK(r.y == 26.0f);  // 下端 = 32（床上辺）にスナップ
    CHECK(r.onGround);
    CHECK_FALSE(r.hitHead);
    CHECK_FALSE(r.hitLeft);
    CHECK_FALSE(r.hitRight);
}

TEST_CASE("MoveAabb pushes back against walls on both sides", "[TileCollision]") {
    // 左端 col 0 と右側 col 4（x = 32..40）が壁
    const LevelIntGrid grid = MakeGrid({
        "#...#.",
        "#...#.",
        "#...#.",
    });
    const Aabb box{12.0f, 4.0f, 6.0f, 6.0f};

    SECTION("+X 移動は壁の左辺で止まり hitRight") {
        const MoveResult r = MoveAabb(grid, box, 40.0f, 0.0f);
        CHECK(r.x == 26.0f);  // 右端 = 32（壁の左辺）
        CHECK(r.hitRight);
        CHECK_FALSE(r.hitLeft);
    }
    SECTION("-X 移動は壁の右辺で止まり hitLeft") {
        const MoveResult r = MoveAabb(grid, box, -40.0f, 0.0f);
        CHECK(r.x == 8.0f);  // 左端 = 8（壁の右辺）
        CHECK(r.hitLeft);
        CHECK_FALSE(r.hitRight);
    }
}

TEST_CASE("MoveAabb hits ceiling when moving up", "[TileCollision]") {
    const LevelIntGrid grid = MakeGrid({
        "####",
        "....",
        "....",
    });
    const Aabb box{4.0f, 12.0f, 6.0f, 6.0f};

    const MoveResult r = MoveAabb(grid, box, 0.0f, -20.0f);
    CHECK(r.y == 8.0f);  // 上端 = 8（天井の下辺）
    CHECK(r.hitHead);
    CHECK_FALSE(r.onGround);
}

TEST_CASE("MoveAabb resolves inner corner without snagging (X pass then Y pass)",
          "[TileCollision]") {
    // 床 row 4 + 右壁 col 4 の内角
    const LevelIntGrid grid = MakeGrid({
        "....#",
        "....#",
        "....#",
        "....#",
        "#####",
    });

    SECTION("斜め移動で角に入ると両軸で止まる") {
        const Aabb box{10.0f, 14.0f, 6.0f, 6.0f};
        const MoveResult r = MoveAabb(grid, box, 40.0f, 40.0f);
        CHECK(r.x == 26.0f);  // 右端 = 32（壁）
        CHECK(r.y == 26.0f);  // 下端 = 32（床）
        CHECK(r.hitRight);
        CHECK(r.onGround);
    }
    SECTION("床の上の横移動は床を壁と誤判定しない") {
        // 下端 = 32 ちょうど（床に接地中）。重力相当の dy 付きで右へ歩く
        const Aabb box{8.0f, 26.0f, 6.0f, 6.0f};
        const MoveResult r = MoveAabb(grid, box, 4.0f, 1.0f);
        CHECK(r.x == 12.0f);  // 横移動は遮られない
        CHECK(r.y == 26.0f);  // 床に留まる
        CHECK(r.onGround);
        CHECK_FALSE(r.hitRight);
        CHECK_FALSE(r.hitLeft);
    }
}

TEST_CASE("MoveAabb does not tunnel through thin tiles at high speed",
          "[TileCollision]") {
    SECTION("1 セル厚の床を 3 セル分の移動で貫通しない") {
        const LevelIntGrid grid = MakeGrid({
            "....",
            "....",
            "####",
            "....",
        });
        const Aabb box{4.0f, 2.0f, 6.0f, 6.0f};  // 下端 = 8、床上辺 = 16
        const MoveResult r = MoveAabb(grid, box, 0.0f, 24.0f);
        CHECK(r.y == 10.0f);  // 下端 = 16 で停止
        CHECK(r.onGround);
    }
    SECTION("1 セル厚の壁を横方向でも貫通しない") {
        const LevelIntGrid grid = MakeGrid({
            "..#.....",
        });
        const Aabb box{2.0f, 1.0f, 6.0f, 6.0f};  // 右端 = 8、壁左辺 = 16
        const MoveResult r = MoveAabb(grid, box, 40.0f, 0.0f);
        CHECK(r.x == 10.0f);  // 右端 = 16 で停止
        CHECK(r.hitRight);
    }
}

TEST_CASE("MoveAabb treats cells outside the grid as empty", "[TileCollision]") {
    const LevelIntGrid grid = MakeGrid({
        "##",
        "##",
    });
    // グリッドの右外・下外を移動しても何にも当たらない
    const Aabb box{40.0f, 40.0f, 6.0f, 6.0f};
    const MoveResult r = MoveAabb(grid, box, 16.0f, 16.0f);
    CHECK(r.x == 56.0f);
    CHECK(r.y == 56.0f);
    CHECK_FALSE(r.hitLeft);
    CHECK_FALSE(r.hitRight);
    CHECK_FALSE(r.hitHead);
    CHECK_FALSE(r.onGround);
}

TEST_CASE("MoveAabb handles negative coordinates without off-by-one",
          "[TileCollision]") {
    // col 0 が壁。負座標側（グリッド外）から右へ移動して壁の左辺 x=0 で…ではなく
    // グリッド外 = 空なので col 0 の左辺 x = 0 に食い込む直前で止まる
    const LevelIntGrid grid = MakeGrid({
        "#...",
        "#...",
    });
    const Aabb box{-20.0f, 2.0f, 6.0f, 6.0f};
    const MoveResult r = MoveAabb(grid, box, 30.0f, 0.0f);
    CHECK(r.x == -6.0f);  // 右端 = 0（壁 col 0 の左辺）
    CHECK(r.hitRight);
}

TEST_CASE("MoveAabb blocks immediately when already touching a boundary",
          "[TileCollision]") {
    const LevelIntGrid grid = MakeGrid({
        "...#",
        "####",
    });

    SECTION("接地中の再着地（重力を毎ステップ与える使い方）") {
        const Aabb box{4.0f, 2.0f, 6.0f, 6.0f};  // 下端 = 8 = 床上辺
        const MoveResult r = MoveAabb(grid, box, 0.0f, 4.0f);
        CHECK(r.y == 2.0f);  // 動かない
        CHECK(r.onGround);
    }
    SECTION("壁に接した状態からの押し付け") {
        const Aabb box{18.0f, 1.0f, 6.0f, 6.0f};  // 右端 = 24 = 壁左辺
        const MoveResult r = MoveAabb(grid, box, 4.0f, 0.0f);
        CHECK(r.x == 18.0f);  // 動かない
        CHECK(r.hitRight);
    }
}

// ── 45° 坂（中心点方式） ────────────────────────────────────────────────────

TEST_CASE("Slope pushes the foot center up to the surface when climbing",
          "[TileCollision][Slope]") {
    const LevelIntGrid grid = MakeRampGrid();
    // 中心 x = 36（坂の中腹、t = 0.5 → 表面 y = 20）。下端 24 = 表面より下に居る
    // （X 移動で坂へ入った直後の状態）。dy が小さくても押し上げられる。
    const Aabb box{33.0f, 18.0f, 6.0f, 6.0f};
    const MoveResult r = MoveAabb(grid, box, 0.0f, 1.0f);
    CHECK(r.y == 14.0f);  // 下端 = 20（表面）
    CHECK(r.onGround);
    CHECK_FALSE(r.hitHead);
}

TEST_CASE("Slope cell does not block horizontal movement", "[TileCollision][Slope]") {
    const LevelIntGrid grid = MakeRampGrid();
    // 地面（下端 24）を右へ歩いて坂セル列 (x 32..40) へ入る。壁扱いされない。
    const Aabb box{24.0f, 18.0f, 6.0f, 6.0f};
    const MoveResult r = MoveAabb(grid, box, 4.0f, 1.0f, /*snapToGround=*/true);
    CHECK(r.x == 28.0f);
    CHECK_FALSE(r.hitRight);
    CHECK(r.onGround);
}

TEST_CASE("Foot-row exclusion does not disable wall checks for a 1-row-tall AABB",
          "[TileCollision][Slope]") {
    // 足元中心セル (col4, row2) が坂で、その右 (col5, row2) が通常のソリッド壁。
    // AABB が縦 1 行にしか跨がらない（h <= gs）とき、足元行除外で検査行が空に
    // なって壁がすり抜けてはならない（レビュー指摘の回帰）。
    const LevelIntGrid grid = MakeGrid({
        "......",
        "......",
        "..../#",  // row2: col4 = 坂, col5 = ソリッド壁
        "######",
    });
    // box: x=33,y=18,w=6,h=6 → rowMin=rowMax=2, footCol=4(坂)。
    const Aabb box{33.0f, 18.0f, 6.0f, 6.0f};
    const MoveResult r = MoveAabb(grid, box, 4.0f, 0.0f);
    CHECK(r.x == 34.0f);  // 右端 = 40（col5 の左辺）で停止（すり抜けない）
    CHECK(r.hitRight);
}

TEST_CASE("Walking up the ramp follows the surface step by step",
          "[TileCollision][Slope]") {
    const LevelIntGrid grid = MakeRampGrid();
    // 地面から坂を登り高台へ。全ステップで onGround を維持し、側面で引っかからない。
    Aabb box{21.0f, 18.0f, 6.0f, 6.0f};  // 中心 24、下端 24（地面）
    bool allGrounded = true;
    bool anyWallHit = false;
    for (int i = 0; i < 24; ++i) {
        const MoveResult r = MoveAabb(grid, box, 1.0f, 1.0f, /*snapToGround=*/true);
        allGrounded = allGrounded && r.onGround;
        anyWallHit = anyWallHit || r.hitLeft || r.hitRight;
        box.x = r.x;
        box.y = r.y;
    }
    CHECK(allGrounded);
    CHECK_FALSE(anyWallHit);
    // 24 ステップ後: 中心 x = 48 は高台 (5,2) の上（上辺 16）→ 下端 16。
    CHECK(box.x == Catch::Approx(45.0f));
    CHECK(box.y == Catch::Approx(10.0f));
}

TEST_CASE("Walking down the ramp stays grounded only with snapToGround",
          "[TileCollision][Slope]") {
    const LevelIntGrid grid = MakeRampGrid();

    SECTION("snapToGround = true なら下りで接地を維持する") {
        // 高台から坂を下って地面へ。
        Aabb box{42.0f, 10.0f, 6.0f, 6.0f};  // 中心 45（高台上）、下端 16
        bool allGrounded = true;
        for (int i = 0; i < 24; ++i) {
            const MoveResult r = MoveAabb(grid, box, -1.0f, 0.2f, /*snapToGround=*/true);
            allGrounded = allGrounded && r.onGround;
            box.x = r.x;
            box.y = r.y;
        }
        CHECK(allGrounded);
        // 中心 x = 21 は平地 → 下端 24。
        CHECK(box.x == Catch::Approx(18.0f));
        CHECK(box.y == Catch::Approx(18.0f));
    }
    SECTION("snapToGround = false だと下りの坂で浮く") {
        const Aabb box{34.0f, 12.0f, 6.0f, 6.0f};  // 中心 37、下端 18 = 表面(t=0.625→19)の 1px 上
        const MoveResult r = MoveAabb(grid, box, -1.0f, 0.2f);
        CHECK_FALSE(r.onGround);  // 表面まで 1px 弱あるので接地しない（吸着なし）
    }
}

TEST_CASE("Fast fall lands on the slope surface without tunneling",
          "[TileCollision][Slope]") {
    const LevelIntGrid grid = MakeRampGrid();
    // 坂の中腹（中心 36、表面 20）の上空から 3 セル分の速度で落下。
    const Aabb box{33.0f, 0.0f, 6.0f, 6.0f};
    const MoveResult r = MoveAabb(grid, box, 0.0f, 24.0f);
    CHECK(r.y == 14.0f);  // 下端 = 20（表面）で停止
    CHECK(r.onGround);
}

TEST_CASE("Standing on a slope surface is stable across steps",
          "[TileCollision][Slope]") {
    const LevelIntGrid grid = MakeRampGrid();
    // 表面ちょうど（中心 36、下端 20）で重力相当の dy を繰り返しても沈まない・浮かない。
    Aabb box{33.0f, 14.0f, 6.0f, 6.0f};
    for (int i = 0; i < 10; ++i) {
        const MoveResult r = MoveAabb(grid, box, 0.0f, 0.5f, /*snapToGround=*/true);
        CHECK(r.onGround);
        box.x = r.x;
        box.y = r.y;
    }
    CHECK(box.y == 14.0f);
}

TEST_CASE("Descending '\\' slope with snap follows the surface",
          "[TileCollision][Slope]") {
    // 高台 (1,2) の右に '\' 坂 (2,2)（表面 16→24）、地面 row 3。
    const LevelIntGrid grid = MakeGrid({
        ".....",
        ".....",
        ".#\\..",
        "#####",
    });
    Aabb box{9.0f, 10.0f, 6.0f, 6.0f};  // 中心 12（高台上）、下端 16
    bool allGrounded = true;
    for (int i = 0; i < 20; ++i) {
        const MoveResult r = MoveAabb(grid, box, 1.0f, 0.2f, /*snapToGround=*/true);
        allGrounded = allGrounded && r.onGround;
        box.x = r.x;
        box.y = r.y;
    }
    CHECK(allGrounded);
    // 中心 x = 32 は地面（row 3 上辺 24）→ 下端 24。
    CHECK(box.x == Catch::Approx(29.0f));
    CHECK(box.y == Catch::Approx(18.0f));
}

TEST_CASE("Snap does not pull down through a solid to a slope below it",
          "[TileCollision][Slope]") {
    // 足元中心列: ソリッドの下に坂。ソリッド上に立っているとき、坂へ吸着しない。
    const LevelIntGrid grid = MakeGrid({
        "...",
        ".#.",
        "./.",
    });
    const Aabb box{9.0f, 2.0f, 6.0f, 6.0f};  // 中心 12、下端 8 = ソリッド (1,1) の上辺
    const MoveResult r = MoveAabb(grid, box, 0.0f, 2.0f, /*snapToGround=*/true);
    CHECK(r.y == 2.0f);  // ソリッド上で停止（坂まで沈まない）
    CHECK(r.onGround);
}

TEST_CASE("Aabb::Overlaps is exclusive at touching edges", "[TileCollision]") {
    const Aabb a{0.0f, 0.0f, 8.0f, 8.0f};
    CHECK(a.Overlaps(Aabb{4.0f, 4.0f, 8.0f, 8.0f}));
    CHECK_FALSE(a.Overlaps(Aabb{8.0f, 0.0f, 8.0f, 8.0f}));  // 辺接触は重なりでない
    CHECK_FALSE(a.Overlaps(Aabb{0.0f, 8.0f, 8.0f, 8.0f}));
    CHECK_FALSE(a.Overlaps(Aabb{20.0f, 0.0f, 8.0f, 8.0f}));
}

TEST_CASE("FindCollisionGrid returns the first IntGrid or null", "[TileCollision]") {
    LevelData level;
    CHECK(FindCollisionGrid(level) == nullptr);

    LevelIntGrid first;
    first.identifier = "IntGrid_layer";
    LevelIntGrid second;
    second.identifier = "Other";
    level.intGrids.push_back(first);
    level.intGrids.push_back(second);

    const LevelIntGrid* found = FindCollisionGrid(level);
    REQUIRE(found != nullptr);
    CHECK(found->identifier == "IntGrid_layer");
}
