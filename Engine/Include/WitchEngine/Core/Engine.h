#pragma once
#include "WitchEngine/Core/Services.h"
#include "WitchEngine/Core/Time.h"
#include "WitchEngine/Rhi/IRenderer.h"
#include "WitchEngine/Scene/Scene.h"
#include <expected>
#include <memory>
#include <string>

namespace witch::vfs {
class Vfs;
} // namespace witch::vfs

namespace witch::log {
class Logger;
} // namespace witch::log

namespace witch::debug {
class DebugDraw;
#ifdef WITCH_DEBUG_UI
class LogViewerWindow;
class HierarchyWindow;
class DebugMenu;
class ProfilerHud;
#endif
} // namespace witch::debug

namespace witch {

class ResourceManager;
class IInput;
class CameraManager;
class GameLoop;

/// エンジン本体。サービスのライフタイムとメインループを管理するシングルトン。
class Engine {
public:
    static Engine& Get();

    /// ウィンドウとサービスを生成し、初期シーンを準備する。
    /// 失敗時（ウィンドウ生成失敗・レンダラ初期化失敗）はエラーメッセージを返す。
    /// 失敗後も Shutdown() は安全に呼べる（生成済みのものだけ逆順破棄される）。
    std::expected<void, std::string> Init(int width = 1280, int height = 720,
                                          const char* title = "Witch");
    /// メインループ。Shutdown() が呼ばれるまでフレームを回し続ける。
    void Run();
    /// サービスを生成の逆順で破棄する。
    void Shutdown();

    /// 次フレーム先頭でシーンを切り替える。
    /// 遷移中に新旧両方が存在しないよう、適用を次フレームに遅延している。
    template<typename T>
    void ChangeScene();

    /// プラットフォーム層がウィンドウ閉鎖を検知したときに呼ぶ。
    void RequestExit() { running_ = false; }

    Services& GetServices() { return Services::Instance(); }

#ifdef WITCH_DEBUG_UI
    /// デバッグウィンドウ外を右クリックしたときのコンテキストメニュー。ゲーム側から
    /// AddItem() で項目を追加できる（"/" 区切りでネスト可）。WITCH_DEBUG_UI 限定。
    debug::DebugMenu* GetDebugMenu() { return debugMenu_.get(); }
#endif

private:
    Engine() = default;
    ~Engine() = default;
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    /// pendingScene_ を currentScene_ に昇格させる。フレーム先頭で呼ぶ。
    void ApplyPendingSceneChange();

    std::unique_ptr<log::Logger>     logger_; ///< 最初に生成し、Shutdown で最後に破棄する
    std::unique_ptr<vfs::Vfs>        vfs_;
    std::unique_ptr<Time>            time_;
    std::unique_ptr<rhi::IRenderer>  renderer_;
    std::unique_ptr<IInput>          input_;
    std::unique_ptr<ResourceManager> resourceManager_;
    std::unique_ptr<CameraManager>   cameraManager_;
    std::unique_ptr<debug::DebugDraw> debugDraw_;
    std::unique_ptr<GameLoop>        gameLoop_;
#ifdef WITCH_DEBUG_UI
    std::unique_ptr<debug::LogViewerWindow> logViewer_;
    std::unique_ptr<debug::HierarchyWindow> hierarchyWindow_;
    std::unique_ptr<debug::DebugMenu>       debugMenu_;
    std::unique_ptr<debug::ProfilerHud>     profilerHud_;
#endif
    std::unique_ptr<Scene>           currentScene_;
    std::unique_ptr<Scene>           pendingScene_;
    bool running_     = false;
    bool initialized_ = false;
};

// ── Template implementation ──────────────────────────────────────────────────

template<typename T>
void Engine::ChangeScene() {
    static_assert(std::is_base_of_v<Scene, T>, "T must derive from witch::Scene");
    pendingScene_ = std::make_unique<T>();
}

} // namespace witch
