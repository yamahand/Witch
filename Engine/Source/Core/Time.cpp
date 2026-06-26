#include "WitchEngine/Core/Time.h"
#include <algorithm>

namespace witch {

void Time::Start() {
    startTime_ = Clock::now();
    lastTime_ = startTime_;
    deltaTime_ = 0.0f;
    totalTime_ = 0.0f;
    frameCount_ = 0;
}

void Time::Tick() {
    auto now = Clock::now();
    using Seconds = std::chrono::duration<float>;
    deltaTime_ = std::chrono::duration_cast<Seconds>(now - lastTime_).count();
    totalTime_ = std::chrono::duration_cast<Seconds>(now - startTime_).count();

    // Clamp delta to avoid spiral of death after pauses (e.g. breakpoints).
    static constexpr float kMaxDelta = 0.25f;
    deltaTime_ = std::min(deltaTime_, kMaxDelta);

    lastTime_ = now;
    ++frameCount_;
}

} // namespace witch
