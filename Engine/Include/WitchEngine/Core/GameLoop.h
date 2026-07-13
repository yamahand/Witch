#pragma once

namespace witch::rhi {
class IRenderer;
} // namespace witch::rhi

#ifdef WITCH_DEBUG_UI
namespace witch::debug {
class LogViewerWindow;
class HierarchyWindow;
class DebugMenu;
class ProfilerHud;
} // namespace witch::debug
#endif

namespace witch {

class Time;
class IInput;
class Scene;

/// 1 フレーム内の位相（入力 → OS メッセージ → 時間更新 → シーン更新 → 描画）を
/// 決まった順序で回すオーケストレータ。以前は Engine::Run に直書きされていた本体を
/// ここへ切り出し、Engine はサービスのライフタイムと while ループの制御に専念する。
///
/// GameLoop はサービスを所有しない。依存（Time / IInput / IRenderer）は Engine から
/// 生ポインタで注入され、いずれも GameLoop より長生きする前提で弱参照として保持する。
/// カメラのビューポート同期は CameraManager サービス（Services 経由）に対して行う。
class GameLoop {
public:
    /// いずれの依存も**非 null 前提**（コンストラクタで assert）。GameLoop は
    /// Engine::Init が成功した場合にのみ生成されるため、この前提は常に成立する。
    /// ヘッドレス実行（renderer なし）は現在サポートしない。必要になったら
    /// null 分岐を戻すのではなく専用の実装を検討すること。
    /// @param time     フレーム時間サービス（非所有、Engine が所有）。
    /// @param input    入力サービス（非所有、Engine が所有）。
    /// @param renderer 描画サービス（非所有、Engine が所有）。
    GameLoop(Time* time, IInput* input, rhi::IRenderer* renderer);

    /// 1 フレーム進める。Engine::Run の while 本体はこれを呼ぶだけにする。
    /// フレーム内の順序（不変条件）は Tick 実装のコメント参照。
    /// @param currentScene 更新対象の現在シーン（null 可）。
    /// @return OS が終了メッセージを送ってきたら false（ループを止める）。
    bool Tick(Scene* currentScene);

#ifdef WITCH_DEBUG_UI
    /// エンジン標準の Log Viewer を設定する（非所有、Engine が所有）。null 可。
    void SetLogViewer(debug::LogViewerWindow* viewer) { logViewer_ = viewer; }
    /// エンジン標準のヒエラルキー＋インスペクターを設定する（非所有、Engine が所有）。null 可。
    void SetHierarchyWindow(debug::HierarchyWindow* window) { hierarchyWindow_ = window; }
    /// デバッグウィンドウ外を右クリックしたときのコンテキストメニューを設定する
    /// （非所有、Engine が所有）。null 可。
    void SetDebugMenu(debug::DebugMenu* menu) { debugMenu_ = menu; }
    /// プロファイラ HUD を設定する（非所有、Engine が所有）。null 可。
    void SetProfilerHud(debug::ProfilerHud* hud) { profilerHud_ = hud; }
#endif

private:
    Time*           time_;
    IInput*         input_;
    rhi::IRenderer* renderer_;
#ifdef WITCH_DEBUG_UI
    debug::LogViewerWindow* logViewer_ = nullptr;
    debug::HierarchyWindow* hierarchyWindow_ = nullptr;
    debug::DebugMenu*       debugMenu_ = nullptr;
    debug::ProfilerHud*     profilerHud_ = nullptr;
#endif
};

} // namespace witch
