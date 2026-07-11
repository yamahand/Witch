#include "Scenes/StageScene.h"
#include "Entities/PlayerObject.h"
#include "Scenes/EmptyScene.h"
#include "WitchEngine/Core/Engine.h"
#include "WitchEngine/Core/Logger.h"
#include "WitchEngine/Core/Services.h"
#include "WitchEngine/Graphics2D/Camera2D.h"
#include "WitchEngine/Graphics2D/CameraManager.h"
#include "WitchEngine/Input/IInput.h"
#include "WitchEngine/Physics2D/TileCollision.h"

namespace witch {

namespace {
constexpr const char* kLevelPath = "Stage/Sample1_1.ldtk";
constexpr float kCameraSpeed    = 100.0f;  // カメラ移動 ワールド単位/秒（8px タイル基準）
constexpr float kZoomSpeed      = 3.0f;    // ズーム変化 /秒（Q/E キー）
constexpr float kWheelZoomStep  = 0.5f;    // ホイール 1 ノッチあたりのズーム変化

/// 「上 2 セルが空 + 下がソリッド」の立てるセルを中央列から左右交互に探す。
/// サンプルレベルに Player エンティティが未配置のための暫定処置で、レベルに
/// Player を置けば LoadLevel の ObjectRegistry 経由（"Player" 登録済み）に
/// 置き換わり、この探索ごと消せる。
bool FindStandableCell(const LevelIntGrid& grid, int& outCx, int& outCy) {
    auto solid = [&](int cx, int cy) {
        if (cx < 0 || cy < 0 || cx >= grid.width || cy >= grid.height) return false;
        const size_t i = static_cast<size_t>(cy) * static_cast<size_t>(grid.width) +
                         static_cast<size_t>(cx);
        return physics2d::ShapeFromValue(grid.values[i]) != physics2d::TileShape::Empty;
    };
    const int center = grid.width / 2;
    for (int offset = 0; offset < grid.width; ++offset) {
        const int cx = (offset % 2 == 0) ? center + offset / 2 : center - (offset / 2 + 1);
        if (cx < 0 || cx >= grid.width) continue;
        for (int cy = 1; cy < grid.height - 1; ++cy) {
            if (!solid(cx, cy) && !solid(cx, cy - 1) && solid(cx, cy + 1)) {
                outCx = cx;
                outCy = cy;
                return true;
            }
        }
    }
    return false;
}
}

void StageScene::OnEnter() {
    log::Info("StageScene: OnEnter");
    log::Info("keys: arrows=player Z=jump WASD=camera Q/E/wheel=zoom "
              "G=reload scene Tab=EmptyScene Esc=quit");

    if (auto result = LoadLevel(kLevelPath); !result) {
        log::Error("StageScene: LoadLevel failed: {}", result.error());
        return;
    }

    // プレイヤーを立てるセルの上に配置（OnEnter 中の Spawn は即時反映）。
    // 位置はコンストラクタ引数で渡す（OnSpawn の自己完結契約）。
    if (const LevelIntGrid* grid = physics2d::FindCollisionGrid(*CurrentLevel())) {
        if (int cx = 0, cy = 0; FindStandableCell(*grid, cx, cy)) {
            const float gs = static_cast<float>(grid->gridSize);
            Spawn<PlayerObject>((static_cast<float>(cx) + 0.5f) * gs,
                                static_cast<float>(cy + 1) * gs -
                                    kPlayerHitboxHeight * 0.5f);
        } else {
            log::Warn("StageScene: no standable cell found — player not spawned");
        }
    } else {
        log::Warn("StageScene: level has no IntGrid — player not spawned");
    }

    // レベル全体が縦方向に収まるようカメラを合わせる（中心注視 + ズーム）。
    if (auto* cameras = Services::Instance().cameras) {
        const LevelData* level = CurrentLevel();
        Camera2D& camera = cameras->Active();
        camera.SetPosition(static_cast<float>(level->width) * 0.5f,
                           static_cast<float>(level->height) * 0.5f);
        // ビューポート（仮想解像度）はこの時点で GameLoop が同期済みでない可能性が
        // あるため、レンダラの仮想解像度から直接求める。
        float zoom = 5.0f;
        if (auto* renderer = Services::Instance().renderer; renderer && level->height > 0) {
            zoom = static_cast<float>(renderer->VirtualHeight()) /
                   static_cast<float>(level->height);
        }
        camera.SetZoom(zoom);
    }
}

void StageScene::FixedUpdate(float fixedDt) {
    IInput* input = Services::Instance().input;
    if (input) {
        // WASD でカメラを移動、Q/E でズーム（カリングの動作確認にも使う）。
        if (auto* cameras = Services::Instance().cameras) {
            Camera2D& camera = cameras->Active();
            float dx = 0.0f;
            float dy = 0.0f;
            if (input->IsDown(Key::A)) dx -= 1.0f;
            if (input->IsDown(Key::D)) dx += 1.0f;
            if (input->IsDown(Key::W)) dy -= 1.0f;
            if (input->IsDown(Key::S)) dy += 1.0f;
            camera.Move(dx * kCameraSpeed * fixedDt, dy * kCameraSpeed * fixedDt);

            if (input->IsDown(Key::E)) camera.SetZoom(camera.Zoom() + kZoomSpeed * fixedDt);
            if (input->IsDown(Key::Q)) camera.SetZoom(camera.Zoom() - kZoomSpeed * fixedDt);
        }
    }
    Scene::FixedUpdate(fixedDt);
}

void StageScene::FrameUpdate(float dt) {
    IInput* input = Services::Instance().input;
    if (input) {
        if (input->WasPressed(Key::Escape)) {
            log::Info("Escape pressed — exiting.");
            Engine::Get().RequestExit();
        }

        // G でシーン再入（レベル・テクスチャの解放 → 再ロードの動作確認）。
        if (input->WasPressed(Key::G)) {
            log::Info("G pressed — reloading StageScene.");
            Engine::Get().ChangeScene<StageScene>();
        }

        // Tab で M5 デモ（EmptyScene）へ遷移（シーン間の行き来の確認）。
        if (input->WasPressed(Key::Tab)) {
            log::Info("Tab pressed — switching to EmptyScene.");
            Engine::Get().ChangeScene<EmptyScene>();
        }

        // マウスホイールでズーム（ホイールは瞬間量なのでフレーム側）。
        if (auto* cameras = Services::Instance().cameras) {
            Camera2D& camera = cameras->Active();
            if (float wheel = input->MouseWheelDelta(); wheel != 0.0f)
                camera.SetZoom(camera.Zoom() + wheel * kWheelZoomStep);
        }
    }
    Scene::FrameUpdate(dt);
}

void StageScene::OnExit() {
    log::Info("StageScene: OnExit");
}

} // namespace witch
