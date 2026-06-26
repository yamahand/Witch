#pragma once
#include <chrono>
#include <cstdint>

namespace witch {

/// フレーム時間を計測するサービス。Services 経由で取得する。
class Time {
public:
    /// タイマーをリセットして開始する。Engine::Init から呼ばれる。
    void Start();
    /// フレーム先頭で呼ぶ。DeltaTime と TotalTime を更新する。
    void Tick();

    /// 直前フレームの経過時間（秒）。
    float DeltaTime() const { return deltaTime_; }
    /// エンジン起動からの累積時間（秒）。
    float TotalTime() const { return totalTime_; }
    /// エンジン起動からのフレーム数。
    uint64_t FrameCount() const { return frameCount_; }

private:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    TimePoint startTime_;
    TimePoint lastTime_;
    float deltaTime_ = 0.0f;
    float totalTime_ = 0.0f;
    uint64_t frameCount_ = 0;
};

} // namespace witch
