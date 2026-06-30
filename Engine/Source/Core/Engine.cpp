#include "WitchEngine/Core/Engine.h"
#include "WitchEngine/Core/Logger.h"
#include "WitchEngine/Core/ResourceManager.h"
#include "Platform/Memory.h"
#include "Platform/PlatformWindow.h"
#include "Rhi/D3D12/D3D12Renderer.h"
#include "Core/Profiling.h"

namespace witch {

static constexpr rhi::Color kCornflowerBlue{0.392f, 0.584f, 0.929f, 1.0f};

Engine& Engine::Get() {
    static Engine instance;
    return instance;
}

void Engine::Init(int width, int height, const char* title) {
    if (initialized_) {
        log::Warn("Engine::Init called more than once.");
        return;
    }

    // アロケータ差し替えが有効か最初に検証する（他サービス生成前）。
    platform::EnsureAllocatorActive();

    log::Info("Engine init start.");

    // Services are created in declaration order; destroyed in reverse during Shutdown.
    time_ = std::make_unique<Time>();
    time_->Start();
    Services::Instance().time = time_.get();

    void* hwnd = platform::CreateMainWindow({width, height, title});

    auto renderer = std::make_unique<D3D12Renderer>();
    if (renderer->Init(hwnd, width, height)) {
        renderer_ = std::move(renderer);
        Services::Instance().renderer = renderer_.get();
    } else {
        log::Error("D3D12Renderer failed to initialize.");
    }

    resourceManager_ = std::make_unique<ResourceManager>();
    Services::Instance().resources = resourceManager_.get();

    initialized_ = true;
    log::Info("Engine init complete.");
}

void Engine::Run() {
    running_ = true;

    while (running_) {
        {
            WITCH_PROFILE_SCOPE_N("Frame");

            ApplyPendingSceneChange();

            {
                WITCH_PROFILE_SCOPE_N("PumpMessages");
                if (!platform::PumpMessages()) {
                    running_ = false;
                    break;
                }
            }

            time_->Tick();

            // 1) 入力を反映しデバッグ UI のフレームを開始（BeginFrame より前に呼べる）。
            if (renderer_) renderer_->BeginDebugUI();

            // 2) ロジック更新。描画器の有無に依存せず、重複なく 1 か所で行う。
            {
                WITCH_PROFILE_SCOPE_N("SceneUpdate");
                if (currentScene_) currentScene_->Update(time_->DeltaTime());
            }

            // 3) 描画（描画器がある場合のみ）。
            if (renderer_) {
                WITCH_PROFILE_SCOPE_N("Render");
                auto* cmdList = renderer_->BeginFrame();
                cmdList->Clear({kCornflowerBlue});
                cmdList->FlushSprites();
                renderer_->RenderDebugUI();
                renderer_->EndFrame(cmdList);
            }
        }

        WITCH_PROFILE_FRAME();
    }
    WITCH_PROFILE_FRAME();

    log::Info("Engine run loop exited.");
}

void Engine::Shutdown() {
    if (currentScene_) {
        currentScene_->OnExit();
        currentScene_.reset();
    }
    pendingScene_.reset();

    // Destroy services in reverse creation order.
    Services::Instance().resources = nullptr;
    resourceManager_.reset();

    if (renderer_) {
        renderer_->Shutdown();
        Services::Instance().renderer = nullptr;
        renderer_.reset();
        log::Info("Renderer destroyed.");
    }

    Services::Instance().time = nullptr;
    time_.reset();
    log::Info("Time destroyed.");

    log::Info("Engine shutdown complete.");
}

void Engine::ApplyPendingSceneChange() {
    if (!pendingScene_)
        return;

    if (currentScene_) {
        currentScene_->OnExit();
        log::Info("Scene exited.");
    }

    currentScene_ = std::move(pendingScene_);
    currentScene_->OnEnter();
    log::Info("Scene entered.");
}

} // namespace witch
