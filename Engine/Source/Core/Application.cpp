#include "WitchEngine/Core/Application.h"
#include "WitchEngine/Core/Engine.h"
#include "WitchEngine/Core/Logger.h"

namespace witch {
    void Application::Run() {
        witch::Engine& engine = witch::Engine::Get();
        int width, height;
        GetWindowSize(width, height);
        auto name = GetGameName();
        auto version = GetGameVersion();
        // name + version をタイトルバーに表示する
        std::string title = std::string(name) + " " + std::string(version);
        engine.Init(width, height, title.c_str());
        OnInit();
        OnStart();
        engine.Run();
        OnShutdown();
        engine.Shutdown();
    }
}