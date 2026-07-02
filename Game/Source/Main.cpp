#include "WitchEngine/Core/Application.h"
#include "WitchEngine/Core/Engine.h"
#include "Scenes/EmptyScene.h"

class WitchGame : public witch::Application {
    public:
        void OnInit() override {
            // 初期化処理
        }

        void OnStart() override {
            // 初期シーンの切り替え
            witch::Engine::Get().ChangeScene<witch::EmptyScene>();
        }

        void OnShutdown() override {
            // 終了処理
        }

        void OnUpdate() override {
            // メインループ処理
        }

        const char* GetGameName() const override {
            return "Witch";
        }

        const char* GetGameVersion() const override {
            return "0.0.1";
        }

        void GetWindowSize(int& width, int& height) const override {
            width = 1280;
            height = 720;
        }
};

int main() {
    WitchGame game;
    game.Run();

    return 0;
}
