#pragma once
#include <cassert>

namespace witch {

/// 固定タイムステップ用のアキュムレータ（Gaffer "Fix Your Timestep" 型）。
/// 時計に依存しない純ロジック。Time が所有し、ループは GameLoop が回す。
///
/// 使い方: フレームごとに Advance(frameDelta) で経過時間を積み、
/// ConsumeStep() が true を返す間 1 固定ステップずつ実行する。
/// 消費し切った後の剰余は常に fixedDelta 未満で、次フレームへ持ち越される
/// （持ち越しはあるが fixedDelta 以上に溜まることはない）。そのため
/// frameDelta に上限（Time の kMaxDelta クランプ）がある限り
/// 1 フレームのステップ数にも自然に上界が付く。
///
/// accumulator は float のため 1/60 の加算誤差が長時間で蓄積しうるが、
/// ステップ数の決定論を要求しない限り実害はない
/// （要求する場合は double か整数ナノ秒に置き換える）。
class FixedStepAccumulator {
public:
    /// fixedDelta は正であること（回復不能な前提違反としてアサートで弾く。
    /// 0 以下だと ConsumeStep が無限ループし、Alpha がゼロ除算になる）。
    explicit constexpr FixedStepAccumulator(float fixedDelta) : fixedDelta_(fixedDelta) {
        assert(fixedDelta > 0.0f && "FixedStepAccumulator: fixedDelta must be positive");
    }

    /// フレームの経過時間を積む。フレームごとに 1 回呼ぶ。
    void Advance(float frameDelta) { accumulator_ += frameDelta; }

    /// 1 固定ステップ分を消費できたら true。false になるまで while で回す。
    bool ConsumeStep() {
        if (accumulator_ < fixedDelta_) {
            return false;
        }
        accumulator_ -= fixedDelta_;
        return true;
    }

    /// 剰余の正規化値。将来の描画補間（前ステップと今ステップのブレンド係数）用。
    /// ConsumeStep() を消費し切った後に呼ぶ契約で、そのとき [0, 1)。
    /// 消費前（Advance 直後など）に呼ぶと 1 以上になり得る。
    float Alpha() const { return accumulator_ / fixedDelta_; }

    /// 1 固定ステップの長さ（秒）。
    float FixedDelta() const { return fixedDelta_; }

    /// 剰余を捨てて初期状態に戻す。Time::Start から呼ばれる。
    void Reset() { accumulator_ = 0.0f; }

private:
    float fixedDelta_;
    float accumulator_ = 0.0f;
};

} // namespace witch
