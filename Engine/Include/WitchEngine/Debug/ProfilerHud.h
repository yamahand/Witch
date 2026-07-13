#pragma once
#ifdef WITCH_DEBUG_UI

namespace witch::debug {

/// インプロセスの ProfileCollector が集めたフレーム時間とゾーン別内訳を
/// ImGui ウィンドウとして描画するプロファイラ HUD。
///
/// Tracy は外部 GUI へストリーム送信するだけでインプロセス読み返し API を持たない
/// ため、この HUD の数値は Profiling.h のマクロ経由で自前集約した ProfileCollector
/// から取る（Tracy のリンク有無に依存しない）。
///
/// 他のデバッグウィンドウと同様、Engine が所有し GameLoop が毎フレーム Draw を呼ぶ。
/// WITCH_DEBUG_UI 定義時のみ存在し、OFF ビルドではヘッダごとビルドから外れる。
class ProfilerHud {
public:
    /// ImGui フレーム内（BeginDebugUI 後・RenderDebugUI 前）で毎フレーム呼ぶ。
    void Draw();

    bool IsOpen() const { return open_; }
    void SetOpen(bool open) { open_ = open; }

private:
    bool open_ = true;       ///< 起動時 ON。閉じても DebugMenu の "Profiler" 項目で再表示できる。
    bool showGraph_ = true;  ///< フレーム時間グラフの表示切替。
    int  sortColumn_ = 2;    ///< 0:Name 1:Calls 2:Last 3:Avg 4:Max（既定 Last の降順）。
    bool sortAscending_ = false;
};

} // namespace witch::debug
#endif // WITCH_DEBUG_UI
