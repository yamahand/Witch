// FixedStepAccumulator（固定タイムステップの純ロジック）の契約テスト。
// 仕様は FixedStepAccumulator.h のコメントに文書化されたもの
// （剰余保持・剰余 < fixedDelta 不変条件・クランプ済み dt によるステップ数上界）。
#include "WitchEngine/Core/FixedStepAccumulator.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace {

using namespace witch;
using Catch::Approx;

constexpr float kFixedDelta = 1.0f / 60.0f;

/// ConsumeStep が false を返すまで回し、消費できたステップ数を返す。
int DrainSteps(FixedStepAccumulator& acc) {
    int steps = 0;
    while (acc.ConsumeStep()) {
        ++steps;
    }
    return steps;
}

TEST_CASE("Frame shorter than fixed delta yields no step and keeps the remainder",
          "[FixedStepAccumulator]") {
    FixedStepAccumulator acc(kFixedDelta);
    acc.Advance(kFixedDelta * 0.5f);

    CHECK(DrainSteps(acc) == 0);
    CHECK(acc.Alpha() == Approx(0.5f));
}

TEST_CASE("Frame of 2.5 fixed deltas yields two steps and alpha 0.5",
          "[FixedStepAccumulator]") {
    FixedStepAccumulator acc(kFixedDelta);
    acc.Advance(kFixedDelta * 2.5f);

    CHECK(DrainSteps(acc) == 2);
    CHECK(acc.Alpha() == Approx(0.5f).margin(1e-4f));
}

TEST_CASE("Exact multiple of fixed delta leaves near-zero remainder",
          "[FixedStepAccumulator]") {
    FixedStepAccumulator acc(kFixedDelta);
    acc.Advance(kFixedDelta * 3.0f);

    CHECK(DrainSteps(acc) == 3);
    // float の丸めで厳密 0 にはならないことがあるため margin で検証する。
    CHECK(acc.Alpha() == Approx(0.0f).margin(1e-4f));
}

TEST_CASE("Small frames accumulate across frames until a step boundary",
          "[FixedStepAccumulator]") {
    FixedStepAccumulator acc(kFixedDelta);

    // 4ms ずつ 4 回（計 16ms < 1/60 * 1000 ≒ 16.67ms）ではまだ届かない。
    for (int i = 0; i < 4; ++i) {
        acc.Advance(0.004f);
        CHECK(DrainSteps(acc) == 0);
    }
    // 5 回目（計 20ms）で境界を跨ぎ、ちょうど 1 ステップ。
    acc.Advance(0.004f);
    CHECK(DrainSteps(acc) == 1);
}

TEST_CASE("Clamped worst-case frame is bounded to a finite step count",
          "[FixedStepAccumulator]") {
    FixedStepAccumulator acc(kFixedDelta);
    // Time::Tick の kMaxDelta = 0.25s クランプが効いた最悪フレーム。
    // 実数では 0.25 / (1/60) = 15 だが、float32 の kFixedDelta は真の 1/60 より
    // わずかに大きく（15 * kFixedDelta > 0.25f）、丸め方向次第で 14 にもなり得る。
    // spiral of death 対策としてはどちらでも十分なので、上界の検証は範囲で行う。
    acc.Advance(0.25f);

    const int steps = DrainSteps(acc);
    CHECK(steps >= 14);
    CHECK(steps <= 15);
    // 不変条件: 消費後の剰余は常に fixedDelta 未満。
    CHECK(acc.Alpha() < 1.0f);
}

TEST_CASE("Reset discards the remainder", "[FixedStepAccumulator]") {
    FixedStepAccumulator acc(kFixedDelta);
    acc.Advance(kFixedDelta * 0.9f);
    acc.Reset();

    CHECK(DrainSteps(acc) == 0);
    CHECK(acc.Alpha() == Approx(0.0f));
}

}  // namespace
