#pragma once
#include "WitchEngine/Core/Log/ILogSink.h"
#include <mutex>
#include <string>
#include <vector>

namespace witch::log {

/// ログを自己完結なコピーとしてリングバッファへ蓄積する Immediate Sink。
/// ImGui 等の表示側が Snapshot() でポーリングする前提の「データ置き場」であり、
/// このクラス自体は描画を一切知らない（ImGui 依存ゼロ。Logger と Viewer の分離点）。
/// 文字列を Write() 時に自前コピーするため、Logger 内部バッファの寿命に依存しない。
class ViewerSink final : public ILogSink {
public:
    /// LogView と同じ形だが文字列を所有するエントリ。
    struct Entry {
        uint64_t sequence = 0;
        std::chrono::system_clock::time_point timestamp;
        uint64_t frameNumber = 0;
        size_t threadId = 0;
        LogLevel level = LogLevel::Info;
        uint64_t categoryHash = 0;
        std::string category;
        std::string message;
        const char* file = "";
        const char* function = "";
        uint32_t line = 0;
    };

    explicit ViewerSink(size_t capacity = 4096);

    [[nodiscard]] SinkMode Mode() const override { return SinkMode::Immediate; }
    void Write(const LogView& view) override;

    /// 現在の内容を古い順にコピーして返す。表示側スレッドから安全に呼べる。
    /// レベル・カテゴリの絞り込みは表示側の責務（ここでは全件保持する）。
    [[nodiscard]] std::vector<Entry> Snapshot() const;

private:
    mutable std::mutex mutex_; ///< Snapshot() が Logger 外のスレッドから呼ばれるため必要
    std::vector<Entry> ring_;
    size_t capacity_;
    size_t head_ = 0; ///< ring_ が満杯のとき、最古エントリ（次の上書き先）の位置
};

} // namespace witch::log
