#include "WitchEngine/Core/Application.h"
#include "WitchEngine/Core/Engine.h"
#include "WitchEngine/Core/Services.h"
#include "WitchEngine/Rhi/IRenderer.h"
#include "WitchGame/Version.h"
#include "GameConfig.h"
#include "Scenes/StageScene.h"

namespace witch {
class WitchGame : public witch::Application {
public:
    void OnInit() override {
        // 基準視界を固定する。ウィンドウサイズによらず同じワールド範囲が見える。
        if (auto* renderer = Services::Instance().renderer) {
            renderer->SetVirtualResolution(kDesignWidth, kDesignHeight);
        }
    }

    void OnStart() override {
        // 初期シーン: M6 タイルマップデモ（Tab で M5 デモの EmptyScene と行き来できる）
        witch::Engine::Get().ChangeScene<witch::StageScene>();
    }

    void OnShutdown() override {
        // 終了処理
    }

    const char* GetGameName() const override {
        return "Witch";
    }

    const char* GetGameVersion() const override {
        return WITCH_GAME_VERSION_STRING;
    }

    void GetWindowSize(int& width, int& height) const override {
        width = 1280;
        height = 720;
    }
};
}

int main() {
    witch::WitchGame game;
    return game.Run();
}
