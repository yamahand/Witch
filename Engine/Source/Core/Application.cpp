#include "Core/Application.h"
#include "Core/Engine.h"

namespace witch {
    void Application::Run() {
        witch::Engine& engine = witch::Engine::Get();
        int width, height;
        GetWindowSize(width, height);
        engine.Init(width, height, GetGameName());
        OnInit();
        OnStart();
        engine.Run();
        OnShutdown();
        engine.Shutdown();
    }
}