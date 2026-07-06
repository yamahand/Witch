#include "WitchEngine/Core/Engine.h"
#include "WitchEngine/Core/GameLoop.h"
#include "WitchEngine/Core/Log/ConsoleSink.h"
#include "WitchEngine/Core/Log/DebugOutputSink.h"
#include "WitchEngine/Core/Log/FileSink.h"
#include "WitchEngine/Core/Log/ViewerSink.h"
#include "WitchEngine/Core/Logger.h"
#include "WitchEngine/Core/ResourceManager.h"
#include "WitchEngine/Graphics2D/CameraManager.h"
#include "WitchEngine/Input/IInput.h"
#include "WitchEngine/Rhi/IRenderer.h"
#include "WitchEngine/Vfs/Vfs.h"
#include "WitchEngine/Core/Version.h"
#include "Platform/Memory.h"
#include "Platform/PlatformWindow.h"
#include "Platform/PlatformFactory.h"
#include "Platform/PlatformPaths.h"
#include "Core/Profiling.h"
#include <filesystem>
#include <format>

namespace witch {

Engine& Engine::Get() {
    static Engine instance;
    return instance;
}

std::expected<void, std::string> Engine::Init(int width, int height, const char* title) {
    if (initialized_) {
        log::Warn("Engine::Init called more than once.");
        return {};
    }

    // アロケータ差し替えが有効か最初に検証する（他サービス生成前）。
    platform::EnsureAllocatorActive();

    // Logger は他サービスより先に生成する。以降の Init 内ログ（他サービス含む）が
    // Sink パイプラインを通るようにするため。Shutdown では逆に最後に破棄する。
    logger_ = std::make_unique<log::Logger>();
    logger_->AddSink(std::make_unique<log::ConsoleSink>());
    logger_->AddSink(std::make_unique<log::DebugOutputSink>());
    logger_->AddSink(std::make_unique<log::ViewerSink>());
    Services::Instance().logger = logger_.get();

    // ファイルログは開けなくても致命傷にしない（dev-Content マウント失敗時と同じ方針）。
    if (auto fileSink = log::FileSink::Create(platform::GetExecutableDir() / "Witch.log")) {
        logger_->AddSink(std::move(*fileSink));
    } else {
        log::Warn("Failed to open log file: {}. Continuing without file logging.",
                  fileSink.error());
    }

    log::Info("Engine init start.");
    log::Info("Engine version: {}", WITCH_ENGINE_VERSION_STRING);

    // Services are created in declaration order; destroyed in reverse during Shutdown.

    // VFS: 実行ファイル直下の Assets ディレクトリをマウントする。
    // WitchGame の CMake POST_BUILD が Game/Assets を $<TARGET_FILE_DIR>/Assets へコピー済み。
    vfs_ = std::make_unique<vfs::Vfs>();
    auto assetsDir = platform::GetExecutableDir() / "Assets";
    if (auto mounted = vfs_->MountDisk(assetsDir); !mounted) {
        return std::unexpected(std::format(
            "Failed to mount Assets directory: {} ({})",
            assetsDir.string(), mounted.error()));
    }

    // 開発ビルドのみ: リポジトリ直下の Content/ をマウントし、コピー無しで直接読む。
    // WITCH_MOUNT_DEV_CONTENT は CMake オプション（既定 OFF、debug 系プリセットで ON）。
    // 配布ビルドは Content の同梱方法が決まるまでこのマウントを持たない。
    // Content/ が未追加の環境（リポジトリにまだコミットされていない等）でも起動できるよう、
    // マウント失敗は警告に留めて続行する（Content 依存アセットの読込は後で失敗するだけ）。
#ifdef WITCH_MOUNT_DEV_CONTENT
    const auto contentDir = std::filesystem::path(WITCH_REPO_ROOT) / "Content";
    if (auto mounted = vfs_->MountDisk(contentDir); !mounted) {
        log::Warn("Failed to mount Content directory: {} ({}). Continuing without it.",
                  contentDir.string(), mounted.error());
    }
#endif
    vfs_->Seal();
    Services::Instance().vfs = vfs_.get();

    time_ = std::make_unique<Time>();
    time_->Start();
    Services::Instance().time = time_.get();

    // 失敗時はその場で返す。ここまでに生成したサービスの後始末は呼び出し側が
    // Shutdown() で行う（Shutdown は未生成メンバを安全にスキップする）。
    // 以前はレンダラ失敗をログだけで握りつぶして headless で走り続けていた。
    void* hwnd = platform::CreateMainWindow({width, height, title});
    if (!hwnd) {
        return std::unexpected(std::string("Failed to create the main window."));
    }

    auto renderer = platform::CreatePlatformRenderer();
    if (!renderer->Init(hwnd, width, height)) {
        // Init が途中まで確保したもの（Win32 イベントハンドル等）を返す前に解放する。
        // Shutdown は部分初期化状態でも安全（実装側で null ガード済み）。
        renderer->Shutdown();
        return std::unexpected(std::string(
            "Failed to initialize the renderer (Direct3D 12).\n"
            "Please make sure your GPU and driver support D3D12."));
    }
    renderer_ = std::move(renderer);
    Services::Instance().renderer = renderer_.get();

    // 入力サービス。WndProc が Services::Instance().input 経由で具象へメッセージを流す。
    input_ = platform::CreatePlatformInput();
    Services::Instance().input = input_.get();

    resourceManager_ = std::make_unique<ResourceManager>();
    Services::Instance().resources = resourceManager_.get();

    // カメラ管理サービス。以前は Scene が Camera2D を直接持っていたが、Scene から独立させた。
    cameraManager_ = std::make_unique<CameraManager>();
    Services::Instance().cameras = cameraManager_.get();

    // フレーム位相のオーケストレータ。全サービス生成後に、依存を注入して作る。
    // Init が成功して返る以上、time_/input_/renderer_ は必ず有効。
    gameLoop_ = std::make_unique<GameLoop>(time_.get(), input_.get(), renderer_.get());

    initialized_ = true;
    log::Info("Engine init complete.");
    return {};
}

void Engine::Run() {
    running_ = true;

    while (running_) {
        // シーン切り替えはフレーム先頭で適用してから当該シーンを回す。
        // （シーン管理の分離は将来 SceneManager へ切り出す予定。）
        // 遷移コスト（OnExit/OnEnter＋リソース読込）は GameLoop の "Frame" ゾーン外だが、
        // 専用スコープで計測し Tracy タイムライン上に残す。
        {
            WITCH_PROFILE_SCOPE_N("SceneChange");
            ApplyPendingSceneChange();
        }

        // 1 フレーム分の位相は GameLoop に委譲する。OS 終了メッセージで false。
        if (!gameLoop_->Tick(currentScene_.get())) {
            running_ = false;
        }

        // フレーム末に Deferred Sink（Console / File）を書き出す。
        logger_->SetFrameNumber(time_->FrameCount());
        logger_->Flush();

        WITCH_PROFILE_FRAME();
    }

    log::Info("Engine run loop exited.");
}

void Engine::Shutdown() {
    // GameLoop は最後に生成したので最初に破棄する（time_/input_/renderer_ を弱参照するため）。
    gameLoop_.reset();

    if (currentScene_) {
        currentScene_->OnExit();
        currentScene_.reset();
    }
    pendingScene_.reset();

    // Destroy services in reverse creation order.
    Services::Instance().cameras = nullptr;
    cameraManager_.reset();

    Services::Instance().resources = nullptr;
    resourceManager_.reset();

    Services::Instance().input = nullptr;
    input_.reset();

    if (renderer_) {
        renderer_->Shutdown();
        Services::Instance().renderer = nullptr;
        renderer_.reset();
        log::Info("Renderer destroyed.");
    }

    Services::Instance().time = nullptr;
    time_.reset();
    log::Info("Time destroyed.");

    Services::Instance().vfs = nullptr;
    vfs_.reset();
    log::Info("Vfs destroyed.");

    log::Info("Engine shutdown complete.");

    // Logger は最後に破棄する（上記のログを Deferred Sink まで届けてから）。
    // 以降の log:: 呼び出しはファサードのフォールバック出力になる。
    if (logger_) {
        logger_->Flush();
        Services::Instance().logger = nullptr;
        logger_.reset();
    }
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
