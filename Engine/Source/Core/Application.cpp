#include "WitchEngine/Core/Application.h"
#include "WitchEngine/Core/Engine.h"
#include "WitchEngine/Core/FixedString.h"
#include "WitchEngine/Core/Logger.h"
#include "Platform/PlatformWindow.h"

#include <string>

namespace witch {
int Application::Run() {
    witch::Engine& engine = witch::Engine::Get();
    int width, height;
    GetWindowSize(width, height);
    auto name = GetGameName();
    auto version = GetGameVersion();
    // name + version をタイトルバーに表示する
    FixedString256 title;
    title.AppendFormat("{} {}", name, version);

    if (auto result = engine.Init(width, height, title.c_str()); !result) {
        // ログはコンソール/デバッガが無いプレイヤー環境では見えないため、
        // 起動失敗は必ずダイアログでも伝える。
        log::Error("Engine::Init failed: {}", result.error());
        platform::ShowErrorDialog(name, result.error().c_str());
        engine.Shutdown(); // 生成済みのサービスだけ逆順破棄される。
        return 1;
    }

    OnInit();
    OnStart();
    engine.Run();
    OnShutdown();
    engine.Shutdown();
    return 0;
}
}
