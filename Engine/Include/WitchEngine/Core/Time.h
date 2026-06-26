#pragma once
#include <chrono>
#include <cstdint>

namespace witch {

class Time {
public:
    void Start();
    void Tick();

    float DeltaTime() const { return deltaTime_; }
    float TotalTime() const { return totalTime_; }
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
