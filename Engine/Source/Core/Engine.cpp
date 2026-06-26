#include "WitchEngine/Core/Engine.h"
#include "WitchEngine/Core/Logger.h"
#include "Platform/PlatformWindow.h"

namespace witch {

Engine& Engine::Get() {
    static Engine instance;
    return instance;
}

void Engine::Init(int width, int height, const char* title) {
    if (initialized_) {
        log::Warn("Engine::Init called more than once.");
        return;
    }

    log::Info("Engine init start.");

    // Services are created in declaration order; destroyed in reverse during Shutdown.
    time_ = std::make_unique<Time>();
    time_->Start();
    Services::Instance().time = time_.get();

    platform::CreateMainWindow({width, height, title});

    initialized_ = true;
    log::Info("Engine init complete.");
}

void Engine::Run() {
    running_ = true;

    while (running_) {
        // Apply a queued scene transition at the head of each frame.
        ApplyPendingSceneChange();

        if (!platform::PumpMessages()) {
            running_ = false;
            break;
        }

        time_->Tick();

        if (currentScene_) {
            currentScene_->Update(time_->DeltaTime());
        }
    }

    log::Info("Engine run loop exited.");
}

void Engine::Shutdown() {
    // Exit current scene.
    if (currentScene_) {
        currentScene_->OnExit();
        currentScene_.reset();
    }
    pendingScene_.reset();

    // Destroy services in reverse creation order.
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
