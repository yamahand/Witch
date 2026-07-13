#pragma once

// インプロセスのプロファイル集約器。
//
// Tracy は計測データを外部 GUI へストリーム送信するだけで、インプロセスで
// 読み返す API を持たない。そのため、インゲームの ProfilerHud に出す数値は
// この自前コレクタが Profiling.h のマクロ経由で集める（Tracy とは独立に動く）。
//
// WITCH_PROFILE_COLLECT が定義されたビルドでのみ実体を持つ（CMakeLists.txt が
// WITCH_DEBUG_UI に連動して定義）。未定義ビルドではこのヘッダは何も定義せず、
// Profiling.h 側でマクロが no-op に潰れるため、このヘッダは include されない。
#ifdef WITCH_PROFILE_COLLECT

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace witch::profile {

/// 1 つの名前付きスコープの、フレーム集計結果のスナップショット（HUD 表示用）。
struct ZoneStat {
    std::string_view name;      ///< スコープ名（マクロに渡した文字列リテラルを指す。非所有）。
    std::uint32_t    calls = 0; ///< このフレーム中に閉じた回数。
    double lastMs = 0.0;        ///< このフレームの合計時間（同名スコープは加算）[ms]。
    double avgMs  = 0.0;        ///< 直近ウィンドウの移動平均（フレーム合計の平均）[ms]。
    double maxMs  = 0.0;        ///< 直近ウィンドウ内でのフレーム合計の最大 [ms]。
};

/// フレーム時間履歴のリングバッファ長（グラフ表示 & 平均/最大の計算窓）。
inline constexpr std::size_t kHistoryFrames = 120;

/// プロセスに 1 つ。ゲームループが BeginFrame を毎フレーム呼び、計測マクロが
/// AddSample で時間を積む。HUD は Snapshot / FrameHistory を読む。
/// スレッド安全ではない（メインスレッドのフレーム計測専用）。
class Collector {
public:
    static Collector& Instance();

    /// フレーム先頭で 1 回呼ぶ。前フレームのゾーン集計を確定し、平均/最大を更新して
    /// 次フレームの蓄積用にリセットする。フレーム全体の経過時間も履歴に積む。
    void BeginFrame();

    /// スコープが閉じるたびに ProfileScope から呼ばれる。同名は加算する。
    /// name は文字列リテラル（静的寿命）である前提で、コピーせずポインタを保持する。
    void AddSample(std::string_view name, std::uint64_t nanoseconds);

    /// 直近で確定した（= 前フレームの）ゾーン集計。HUD 描画用。
    const std::vector<ZoneStat>& Snapshot() const { return snapshot_; }

    /// フレーム全体時間の履歴を時系列順（古い→新しい）に並べて out へ書き出す。
    /// 返り値は書き込んだ要素数（起動直後は kHistoryFrames 未満）。out は
    /// kHistoryFrames 要素を持つこと。リングバッファの折り返しを解いて渡すため、
    /// グラフ描画側は out の先頭からそのまま PlotLines へ渡せる。
    std::size_t FrameHistoryOrdered(std::array<float, kHistoryFrames>& out) const;

    /// 直近フレームの全体時間 [ms]（履歴が空なら 0）。
    float LastFrameMs() const;

    /// スパイク検出ログの有効/無効を設定する。有効時、フレーム全体時間が
    /// thresholdMs を超えたフレームの直後（次の BeginFrame）に、そのフレームの
    /// ゾーン内訳を Logger へ Warn 出力する。原因ゾーンと発生タイミングを掴む用。
    void SetSpikeLog(bool enabled, double thresholdMs) {
        spikeLogEnabled_ = enabled;
        spikeThresholdMs_ = thresholdMs;
    }
    bool SpikeLogEnabled() const { return spikeLogEnabled_; }
    double SpikeThresholdMs() const { return spikeThresholdMs_; }

private:
    Collector() = default;

    using Clock = std::chrono::steady_clock;

    /// 蓄積中（現フレーム）のゾーン。name をキーに線形検索する（数十件程度で十分軽い）。
    struct Accum {
        std::string_view name;
        std::uint64_t    totalNs = 0;
        std::uint32_t    calls   = 0;
        double avgMs = 0.0; ///< 移動平均（フレームをまたいで保持）。
        double maxMs = 0.0; ///< 移動最大（フレームをまたいで保持）。
    };

    std::vector<Accum>    accum_;    ///< 現フレームの蓄積（BeginFrame で calls/totalNs をリセット）。
    std::vector<ZoneStat> snapshot_; ///< 前フレームの確定結果。

    std::array<float, kHistoryFrames> frameMs_{};
    std::size_t frameCount_ = 0; ///< 累積フレーム数（履歴書き込み位置 = frameCount_ % kHistoryFrames）。
    Clock::time_point lastFrameStart_{};
    bool hasFrameStart_ = false;

    bool   spikeLogEnabled_  = false;
    double spikeThresholdMs_ = 8.0; ///< この値を超えたフレームをスパイクとしてログ出力。

    static constexpr double kAvgSmoothing = 0.05; ///< 指数移動平均の係数（小さいほど滑らか）。
};

/// RAII スコープ計測。閉じるときに Collector::AddSample を呼ぶ。
/// Profiling.h の WITCH_PROFILE_SCOPE_N がこれを 1 個スタックに積む。
class ProfileScope {
public:
    explicit ProfileScope(std::string_view name)
        : name_(name), start_(std::chrono::steady_clock::now()) {}

    ~ProfileScope() {
        const auto elapsed = std::chrono::steady_clock::now() - start_;
        const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count();
        Collector::Instance().AddSample(name_, static_cast<std::uint64_t>(ns));
    }

    ProfileScope(const ProfileScope&) = delete;
    ProfileScope& operator=(const ProfileScope&) = delete;

private:
    std::string_view                     name_;
    std::chrono::steady_clock::time_point start_;
};

} // namespace witch::profile

#endif // WITCH_PROFILE_COLLECT
