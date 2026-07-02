#pragma once

namespace witch::rhi {
class IRenderer;
} // namespace witch::rhi

namespace witch {

class Time;
class IInput;
class Scene;

/// 1 フレーム内の位相（入力 → OS メッセージ → 時間更新 → シーン更新 → 描画）を
/// 決まった順序で回すオーケストレータ。以前は Engine::Run に直書きされていた本体を
/// ここへ切り出し、Engine はサービスのライフタイムと while ループの制御に専念する。
///
/// GameLoop はサービスを所有しない。依存（Time / IInput / IRenderer）は Engine から
/// 生ポインタで注入され、いずれも GameLoop より長生きする前提で弱参照として保持する。
/// カメラのビューポート同期は CameraManager サービス（Services 経由）に対して行う。
class GameLoop {
public:
    /// @param time     フレーム時間サービス（非所有、Engine が所有）。
    /// @param input    入力サービス（非所有、Engine が所有）。
    /// @param renderer 描画サービス（非所有、null 可＝ヘッドレス相当）。
    GameLoop(Time* time, IInput* input, rhi::IRenderer* renderer);

    /// 1 フレーム進める。Engine::Run の while 本体はこれを呼ぶだけにする。
    /// フレーム内の順序（不変条件）は Tick 実装のコメント参照。
    /// @param currentScene 更新対象の現在シーン（null 可）。
    /// @return OS が終了メッセージを送ってきたら false（ループを止める）。
    bool Tick(Scene* currentScene);

private:
    Time*           time_;
    IInput*         input_;
    rhi::IRenderer* renderer_;
};

} // namespace witch
