#pragma once

namespace witch {

/// アプリケーションの基礎クラス。Engine のラッパーとして、ユーザーが継承して使用する。
class Application {
public:
    virtual ~Application() = default;

    /// エンジンを初期化してメインループを回す。
    /// @return プロセス終了コード。初期化失敗時はエラーダイアログを表示して 1 を返す。
    int Run();

    /// アプリケーションの初期化処理。Engine::Init() の後に呼ばれる。
    virtual void OnInit() = 0;

    /// アプリケーションの開始処理。初期化処理の後に呼ばれる。初期シーンの切り替えはここで行う。
    virtual void OnStart() = 0;

    /// アプリケーションの終了処理。Engine::Shutdown() の前に呼ばれる。
    virtual void OnShutdown() = 0;

    /// アプリケーションの名前を返す。
    virtual const char* GetGameName() const = 0;

    /// アプリケーションのバージョンを返す。
    virtual const char* GetGameVersion() const = 0;

    /// アプリケーションのウィンドウサイズを返す。
    virtual void GetWindowSize(int& width, int& height) const = 0;

};

}
