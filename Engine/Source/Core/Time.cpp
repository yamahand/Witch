#include "WitchEngine/Core/Time.h"
#include <algorithm>

namespace witch {

void Time::Start() {
    startTime_ = Clock::now();
    lastTime_ = startTime_;
    deltaTime_ = 0.0f;
    totalTime_ = 0.0f;
    frameCount_ = 0;
    accumulator_.Reset();
}

void Time::Tick() {
    auto now = Clock::now();
    using Seconds = std::chrono::duration<float>;
    deltaTime_ = std::chrono::duration_cast<Seconds>(now - lastTime_).count();
    totalTime_ = std::chrono::duration_cast<Seconds>(now - startTime_).count();

    // Clamp delta to avoid spiral of death after pauses (e.g. breakpoints).
    // 固定タイムステップ導入後もこのクランプが 1 フレームの固定ステップ数の
    // 上界（0.25 / kFixedDelta = 15 回）を与える主対策として残る。
    static constexpr float kMaxDelta = 0.25f;
    deltaTime_ = std::min(deltaTime_, kMaxDelta);

    // クランプ後の dt を固定ステップのアキュムレータに積む（クランプ前だと
    // ブレークポイント等の長時間停止で大量ステップが走ってしまう）。
    accumulator_.Advance(deltaTime_);

    lastTime_ = now;
    ++frameCount_;
}

} // namespace witch
