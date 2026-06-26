#pragma once
#include "WitchEngine/Core/Services.h"
#include "WitchEngine/Core/Time.h"
#include "WitchEngine/Rhi/IRenderer.h"
#include "WitchEngine/Scene/Scene.h"
#include <memory>

namespace witch {

class ResourceManager;

class Engine {
public:
    static Engine& Get();

    void Init(int width = 1280, int height = 720, const char* title = "Witch");
    void Run();
    void Shutdown();

    // Queues a scene transition; applied at the start of the next frame.
    template<typename T>
    void ChangeScene();

    // Called by the platform layer when the window is closed.
    void RequestExit() { running_ = false; }

    Services& GetServices() { return Services::Instance(); }

private:
    Engine() = default;
    ~Engine() = default;
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    void ApplyPendingSceneChange();

    std::unique_ptr<Time>            time_;
    std::unique_ptr<rhi::IRenderer>  renderer_;
    std::unique_ptr<ResourceManager> resourceManager_;
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
