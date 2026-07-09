#pragma once
#include "WitchEngine/Core/FixedStepAccumulator.h"
#include <chrono>
#include <cstdint>

namespace witch {

/// フレーム時間を計測するサービス。Services 経由で取得する。
class Time {
public:
    /// 固定タイムステップの周波数（60Hz）。変更する場合もここ 1 箇所。
    /// 消費側は必ず FixedDeltaTime() 経由で読むこと（将来の実行時可変化に備える）。
    static constexpr float kFixedDelta = 1.0f / 60.0f;

    /// タイマーをリセットして開始する。Engine::Init から呼ばれる。
    void Start();
    /// フレーム先頭で呼ぶ。DeltaTime と TotalTime を更新する。
    void Tick();

    /// 直前フレームの経過時間（秒）。
    float DeltaTime() const { return deltaTime_; }
    /// エンジン起動からの累積時間（秒）。
    float TotalTime() const { return totalTime_; }
    /// エンジン起動からのフレーム数（描画フレーム単位。固定ステップ数ではない）。
    uint64_t FrameCount() const { return frameCount_; }

    /// 1 固定ステップの長さ（秒）。Scene::FixedUpdate に渡される dt はこの値。
    float FixedDeltaTime() const { return accumulator_.FixedDelta(); }
    /// 1 固定ステップ分を消費できたら true。GameLoop 専用（フレーム内で while で回す）。
    /// Tick() が dt をクランプ（kMaxDelta）してから積むため、1 フレームの
    /// ステップ数には自然に上界が付く（FixedStepAccumulator.h の契約参照）。
    bool ConsumeFixedStep() { return accumulator_.ConsumeStep(); }
    /// 固定ステップの剰余の正規化値 [0, 1)。将来の描画補間用（現状消費者なし）。
    float InterpolationAlpha() const { return accumulator_.Alpha(); }

private:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    TimePoint startTime_;
    TimePoint lastTime_;
    float deltaTime_ = 0.0f;
    float totalTime_ = 0.0f;
    uint64_t frameCount_ = 0;
    FixedStepAccumulator accumulator_{kFixedDelta};
};

} // namespace witch
