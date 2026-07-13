#ifdef WITCH_PROFILE_COLLECT
#include "Core/ProfileCollector.h"

namespace witch::profile {

Collector& Collector::Instance() {
    static Collector instance;
    return instance;
}

void Collector::BeginFrame() {
    // フレーム全体時間: 前回 BeginFrame からの経過を履歴に積む。
    const auto now = Clock::now();
    if (hasFrameStart_) {
        const auto elapsed = now - lastFrameStart_;
        const double ms =
            std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(elapsed).count();
        frameMs_[frameCount_ % kHistoryFrames] = static_cast<float>(ms);
        ++frameCount_;
    }
    lastFrameStart_ = now;
    hasFrameStart_ = true;

    // 前フレームの蓄積を確定してスナップショットへ移す。あわせて平均/最大を更新。
    snapshot_.clear();
    snapshot_.reserve(accum_.size());
    for (auto& a : accum_) {
        const double frameMs = static_cast<double>(a.totalNs) / 1'000'000.0;

        // 指数移動平均。初回（avgMs==0 かつ呼ばれた）は素直に今回値で埋める。
        if (a.avgMs <= 0.0) {
            a.avgMs = frameMs;
        } else {
            a.avgMs += (frameMs - a.avgMs) * kAvgSmoothing;
        }
        if (frameMs > a.maxMs) {
            a.maxMs = frameMs;
        }

        // このフレームに一度も現れなかったスコープは calls==0。表示はするが、
        // 蓄積のみリセットして平均/最大は保持する（一時的に消えても履歴を残す）。
        snapshot_.push_back(ZoneStat{
            .name = a.name,
            .calls = a.calls,
            .lastMs = frameMs,
            .avgMs = a.avgMs,
            .maxMs = a.maxMs,
        });

        // 次フレーム用にフレーム蓄積だけリセット（平均/最大はまたいで保持）。
        a.totalNs = 0;
        a.calls = 0;
    }
}

std::size_t Collector::FrameHistoryOrdered(std::array<float, kHistoryFrames>& out) const {
    const std::size_t count = frameCount_ < kHistoryFrames ? frameCount_ : kHistoryFrames;
    if (count < kHistoryFrames) {
        // まだ一巡していない: 書き込みは 0..count-1 に時系列で並んでいる。
        for (std::size_t i = 0; i < count; ++i) {
            out[i] = frameMs_[i];
        }
    } else {
        // 一巡済み: 最古の要素は次に上書きされる位置 = frameCount_ % N。
        const std::size_t oldest = frameCount_ % kHistoryFrames;
        for (std::size_t i = 0; i < kHistoryFrames; ++i) {
            out[i] = frameMs_[(oldest + i) % kHistoryFrames];
        }
    }
    return count;
}

float Collector::LastFrameMs() const {
    if (frameCount_ == 0) {
        return 0.0f;
    }
    // 直近に書いた位置は (frameCount_ - 1) % N。
    return frameMs_[(frameCount_ - 1) % kHistoryFrames];
}

void Collector::AddSample(std::string_view name, std::uint64_t nanoseconds) {
    // 同名スコープを線形検索して加算。name は文字列リテラル前提なのでポインタ比較で足りるが、
    // 念のため内容比較（同一リテラルでも TU をまたぐと別実体になり得るため）。
    for (auto& a : accum_) {
        if (a.name == name) {
            a.totalNs += nanoseconds;
            ++a.calls;
            return;
        }
    }
    accum_.push_back(Accum{.name = name, .totalNs = nanoseconds, .calls = 1});
}

} // namespace witch::profile
#endif // WITCH_PROFILE_COLLECT
