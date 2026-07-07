#pragma once
#ifdef WITCH_DEBUG_UI
#include "WitchEngine/Core/Log/LogLevel.h"
#include <cstddef>

namespace witch::log {
class ViewerSink;
} // namespace witch::log

namespace witch::debug {

/// ViewerSink の内容を ImGui ウィンドウとして描画する Log Viewer。
/// Logger 側は本クラスを知らない（依存は Viewer → ViewerSink の一方向のみ）。
/// WITCH_DEBUG_UI 定義時のみ存在し、OFF ビルドではヘッダごとビルドから外れる。
class LogViewerWindow {
public:
    /// @param sink 表示対象（非所有）。Logger が所有する ViewerSink を Engine が渡す。
    ///             Logger は本ウィンドウより後に破棄されるため、常に有効。
    explicit LogViewerWindow(log::ViewerSink* sink) : sink_(sink) {}

    /// ImGui フレーム内（BeginDebugUI 後・RenderDebugUI 前）で毎フレーム呼ぶ。
    void Draw();

    bool IsOpen() const { return open_; }
    void SetOpen(bool open) { open_ = open; }

private:
    static constexpr int kLevelCount = 5; // Trace / Info / Warn / Error / Fatal

    log::ViewerSink* sink_;
    bool open_ = true;
    bool autoScroll_ = true;
    // レベルごとの表示 ON/OFF。ON のレベルだけを表示する（閾値ではなく個別トグル）。
    bool levelEnabled_[kLevelCount] = {true, true, true, true, true};
    // ログ本文に対する部分一致フィルタ。空なら全件通す。
    char filter_[64] = {};
};

} // namespace witch::debug
#endif // WITCH_DEBUG_UI
