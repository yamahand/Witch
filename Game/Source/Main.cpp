#include "WitchEngine/Core/Engine.h"
#include "Scenes/EmptyScene.h"

int main() {
    witch::Engine& engine = witch::Engine::Get();

    engine.Init(1280, 720, "Witch");
    engine.ChangeScene<witch::EmptyScene>();
    engine.Run();
    engine.Shutdown();

    return 0;
}
