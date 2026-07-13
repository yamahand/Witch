#include "Scenes/EmptyScene.h"
#include "Scenes/StageScene.h"
#include "WitchEngine/Core/Engine.h"
#include "WitchEngine/Core/Logger.h"
#include "WitchEngine/Core/Services.h"
#include "WitchEngine/Core/ResourceManager.h"
#include "WitchEngine/Graphics2D/AnimationComponent.h"
#include "WitchEngine/Graphics2D/AsepriteComponent.h"
#include "WitchEngine/Graphics2D/Camera2D.h"
#include "WitchEngine/Graphics2D/CameraManager.h"
#include "WitchEngine/Graphics2D/SpriteComponent.h"
#include "WitchEngine/Input/IInput.h"
#include "WitchEngine/Rhi/IRenderer.h"
#include <iterator>

namespace witch {

namespace {
constexpr float kMoveSpeed     = 300.0f; // スプライト移動 ピクセル/秒（ワールド）
constexpr float kCameraSpeed   = 400.0f; // カメラ移動 ワールド単位/秒
constexpr float kZoomSpeed     = 1.5f;   // ズーム変化 /秒（Q/E キー）
constexpr float kWheelZoomStep = 0.1f;   // ホイール 1 ノッチあたりのズーム変化
constexpr float kSpinSpeed     = 1.5f;   // R 押下中の回転速度 ラジアン/秒

// T キーで循環する tint（白 → 赤 → 半透明白）。
constexpr rhi::Color kTints[] = {
    {1.0f, 1.0f, 1.0f, 1.0f},
    {1.0f, 0.3f, 0.3f, 1.0f},
    {1.0f, 1.0f, 1.0f, 0.5f},
};

// N キーで循環する Unity ちゃんの .ase（Content マウント直下からの VFS パス）。
constexpr const char* kUnitychanAseFiles[] = {
    "Aseprite/Unitychan/Unitychan_Idle.ase",
    "Aseprite/Unitychan/Unitychan_Run.ase",
    "Aseprite/Unitychan/Unitychan_Attack1.ase",
    "Aseprite/Unitychan/Unitychan_Jump_Up.ase",
};
}

void EmptyScene::OnEnter() {
    log::Info("EmptyScene: OnEnter");
    log::Info("keys: arrows=move WASD=camera Q/E/wheel=zoom R=spin F=flip "
              "T=tint L=layer P=anim play/stop O=anim loop N=unitychan anim "
              "G=reload scene Tab=StageScene Esc=quit");

    auto* resources = Services::Instance().resources;

    auto witchTex = resources->LoadTexture("Witch.png");
    if (!witchTex) {
        log::Error("Failed to load sprite: {}", witchTex.error());
        return;
    }
    spriteTexture_ = *witchTex;

    auto sheetTex = resources->LoadTexture("TestSheet.png");
    if (!sheetTex) {
        log::Error("Failed to load test sheet: {}", sheetTex.error());
        return;
    }
    testSheet_ = *sheetTex;

    // 操作対象の Witch（ワールド原点、レイヤー +1 = 手前スタート）。
    auto* obj = Spawn<GameObject>();
    obj->transform.x = 0.0f;
    obj->transform.y = 0.0f;
    witchSprite_ = obj->AddComponent<SpriteComponent>(spriteTexture_, 128.0f, 128.0f);
    witchSprite_->SetLayer(1);

    // レイヤー検証用の静止 Witch（操作対象と重なる位置、レイヤー 0）。
    auto* staticObj = Spawn<GameObject>();
    staticObj->transform.x = 64.0f;
    staticObj->transform.y = 64.0f;
    staticSprite_ = staticObj->AddComponent<SpriteComponent>(spriteTexture_, 128.0f, 128.0f);
    staticSprite_->SetColor({0.7f, 0.7f, 1.0f, 1.0f}); // 薄青で識別

    // TestSheet アニメーション（赤→緑→青→黄 4fps ループ）。
    auto* animObj = Spawn<GameObject>();
    animObj->transform.x = 250.0f;
    animObj->transform.y = 0.0f;
    anim_ = animObj->AddComponent<AnimationComponent>(
        AnimationClip{.frameWidth = 32, .frameHeight = 32, .columns = 4,
                      .frames = {0, 1, 2, 3}, .fps = 4.0f, .loop = true});
    animObj->AddComponent<SpriteComponent>(testSheet_, 64.0f, 64.0f);

    // Aseprite デモ: Unity ちゃんの .ase を直接ロードして再生（Content マウント経由）。
    // シートごとにコマ数・コマごとの duration が異なる。N キーで切り替える。
    for (const char* path : kUnitychanAseFiles) {
        auto sheet = resources->LoadAseprite(path);
        if (!sheet) {
            log::Error("Failed to load aseprite {}: {}", path, sheet.error());
            continue;
        }
        unitySheets_.push_back({path, *sheet});
    }
    if (!unitySheets_.empty()) {
        auto* unityObj = Spawn<GameObject>();
        unityObj->transform.x = 250.0f;
        unityObj->transform.y = 150.0f;
        unityAnim_ = unityObj->AddComponent<AsepriteComponent>(unitySheets_[0].sheet);
        // 描画サイズは 72x72 の 2 倍。80x80 のシートへ切り替えても同サイズで描く（デモ用）。
        unityObj->AddComponent<SpriteComponent>(unitySheets_[0].sheet->texture, 144.0f, 144.0f);
    }

    // HUD スプライト（Screen 空間 = 仮想座標直指定、カメラ非追従）。
    // シーン領域の左上 (16,16) に TestSheet の 0 コマ目を貼る。
    auto* hudObj = Spawn<GameObject>();
    hudObj->transform.x = 16.0f;
    hudObj->transform.y = 16.0f;
    auto* hud = hudObj->AddComponent<SpriteComponent>(testSheet_, 64.0f, 64.0f);
    hud->SetSpace(SpriteSpace::Screen);
    hud->SetSourceRect(0, 0, 32, 32);

    // カメラを Witch の中心あたりに向ける（スプライト左上が原点なので +64 で中心）。
    if (auto* cameras = Services::Instance().cameras) {
        Camera2D& camera = cameras->Active();
        camera.SetPosition(64.0f, 64.0f);
        camera.SetZoom(1.0f);
    }
}

void EmptyScene::FixedUpdate(float fixedDt) {
    // 連続量の入力（IsDown + dt スケール）は固定ステップ側で処理する。
    // 移動・回転速度が描画レートに依存せず 60Hz 固定で決まる。
    IInput* input = Services::Instance().input;
    if (input) {
        // 矢印キーで Witch スプライトを移動。オーナー GameObject はシーンと同寿命
        // （Destroy しない）ので、Spawn 時に保持した witchSprite_ の Owner() から直接引く。
        if (GameObject* witch = witchSprite_ ? witchSprite_->Owner() : nullptr) {
            float dx = 0.0f;
            float dy = 0.0f;
            if (input->IsDown(Key::Left))  dx -= 1.0f;
            if (input->IsDown(Key::Right)) dx += 1.0f;
            if (input->IsDown(Key::Up))    dy -= 1.0f;
            if (input->IsDown(Key::Down))  dy += 1.0f;
            witch->transform.x += dx * kMoveSpeed * fixedDt;
            witch->transform.y += dy * kMoveSpeed * fixedDt;

            // R 押下中は反時計回りに回転（ピボット = アンカー = 中心）。
            if (input->IsDown(Key::R))
                witch->transform.rotation += kSpinSpeed * fixedDt;
        }

        // WASD でカメラを移動、Q/E でズーム（カメラ／座標系の動作確認）。
        if (auto* cameras = Services::Instance().cameras) {
            Camera2D& camera = cameras->Active();
            float cdx = 0.0f;
            float cdy = 0.0f;
            if (input->IsDown(Key::A)) cdx -= 1.0f;
            if (input->IsDown(Key::D)) cdx += 1.0f;
            if (input->IsDown(Key::W)) cdy -= 1.0f;
            if (input->IsDown(Key::S)) cdy += 1.0f;
            camera.Move(cdx * kCameraSpeed * fixedDt, cdy * kCameraSpeed * fixedDt);

            if (input->IsDown(Key::E)) camera.SetZoom(camera.Zoom() + kZoomSpeed * fixedDt);
            if (input->IsDown(Key::Q)) camera.SetZoom(camera.Zoom() - kZoomSpeed * fixedDt);
        }
    }
    Scene::FixedUpdate(fixedDt);
}

void EmptyScene::FrameUpdate(float dt) {
    ++frameCount_;

    // エッジ/瞬間量の入力（WasPressed / ホイール）はフレーム側で処理する。
    // 固定側に置くと多重ステップフレームで二重発火する（EmptyScene.h 参照）。
    IInput* input = Services::Instance().input;
    if (input) {
        // Escape でアプリ終了（WasPressed のエッジ検出を確認）。
        if (input->WasPressed(Key::Escape)) {
            log::Info("Escape pressed — exiting.");
            Engine::Get().RequestExit();
        }

        // G でシーン再入（テクスチャ解放 → 再ロードの動作確認。RefactoringNotes §5）。
        if (input->WasPressed(Key::G)) {
            log::Info("G pressed — reloading EmptyScene.");
            Engine::Get().ChangeScene<EmptyScene>();
        }

        // Tab で M6 デモ（StageScene）へ遷移。
        if (input->WasPressed(Key::Tab)) {
            log::Info("Tab pressed — switching to StageScene.");
            Engine::Get().ChangeScene<StageScene>();
        }

        // F/T/L で M5 描画機能を切り替え。
        if (witchSprite_) {
            if (input->WasPressed(Key::F))
                witchSprite_->SetFlip(!witchSprite_->FlipX(), false);
            if (input->WasPressed(Key::T)) {
                tintIndex_ = (tintIndex_ + 1) % static_cast<int>(std::size(kTints));
                witchSprite_->SetColor(kTints[tintIndex_]);
            }
            if (input->WasPressed(Key::L)) {
                // 静止 Witch (レイヤー0) との前後を入れ替える。
                witchSprite_->SetLayer(witchSprite_->Layer() > 0 ? -1 : 1);
                log::Info("witch layer = {}", witchSprite_->Layer());
            }
        }

        // アニメーション操作。
        if (anim_) {
            if (input->WasPressed(Key::P)) {
                if (anim_->IsPlaying()) anim_->Stop();
                else                    anim_->Play();
                loggedFinished_ = false;
            }
            if (input->WasPressed(Key::O)) {
                animLoop_ = !animLoop_;
                anim_->SetClip(AnimationClip{.frameWidth = 32, .frameHeight = 32,
                                             .columns = 4, .frames = {0, 1, 2, 3},
                                             .fps = 4.0f, .loop = animLoop_});
                loggedFinished_ = false;
                log::Info("anim loop = {}", animLoop_);
            }
            if (anim_->IsFinished() && !loggedFinished_) {
                log::Info("anim finished (frame {})", anim_->CurrentFrame());
                loggedFinished_ = true;
            }
        }

        // N キーで Unity ちゃんのシートを循環（テクスチャごと差し替わる）。
        if (unityAnim_ && !unitySheets_.empty() && input->WasPressed(Key::N)) {
            unitySheetIndex_ = (unitySheetIndex_ + 1) % static_cast<int>(unitySheets_.size());
            const auto& entry = unitySheets_[static_cast<size_t>(unitySheetIndex_)];
            unityAnim_->SetSheet(entry.sheet);
            log::Info("unitychan anim -> {}", entry.path);
        }

        // マウスホイールでズーム（ホイールは瞬間量なのでフレーム側）。
        if (auto* cameras = Services::Instance().cameras) {
            Camera2D& camera = cameras->Active();
            if (float wheel = input->MouseWheelDelta(); wheel != 0.0f)
                camera.SetZoom(camera.Zoom() + wheel * kWheelZoomStep);
        }
    }
#if 0
    if (frameCount_ % 60 == 1 && input) {
        // マウス座標の 3 系統: ウィンドウ実ピクセル → 仮想解像度 → ワールド。
        // IInput はウィンドウ座標のまま。仮想・ワールドへの変換はゲーム側で明示する。
        const float mx = input->MouseX();
        const float my = input->MouseY();
        float vx = mx, vy = my, wx = mx, wy = my;
        if (auto* renderer = Services::Instance().renderer) {
            vx = renderer->WindowToVirtualX(mx);
            vy = renderer->WindowToVirtualY(my);
        }
        if (auto* cameras = Services::Instance().cameras) {
            wx = cameras->Active().ScreenToWorldX(vx);
            wy = cameras->Active().ScreenToWorldY(vy);
        }
        log::Info("EmptyScene frame {} (dt={:.4f}s) mouse win=({:.0f},{:.0f}) "
                  "virt=({:.0f},{:.0f}) world=({:.0f},{:.0f})",
                  frameCount_, dt, mx, my, vx, vy, wx, wy);
    }
#endif
    Scene::FrameUpdate(dt);
}

void EmptyScene::OnExit() {
    log::Info("EmptyScene: OnExit");
}

} // namespace witch
