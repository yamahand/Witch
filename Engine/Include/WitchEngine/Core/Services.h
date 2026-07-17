#pragma once

namespace witch::rhi {
class IRenderer;
} // namespace witch::rhi

namespace witch::vfs {
class Vfs;
} // namespace witch::vfs

namespace witch::log {
class Logger;
} // namespace witch::log

namespace witch::debug {
class DebugDraw;
} // namespace witch::debug

namespace witch::audio {
class IAudio;
} // namespace witch::audio

namespace witch {

class Time;
class ResourceManager;
class IInput;
class CameraManager;

/// エンジンが所有するサービスへの非所有ビュー。
/// Engine が Init で各ポインタを設定し、Shutdown でクリアする。
/// サービスを直接 Singleton にせずここで一元管理することで、生成・破棄順を Engine が握る。
struct Services {
    log::Logger*     logger    = nullptr; ///< 最初に生成され最後に破棄される（Engine 参照）
    rhi::IRenderer*  renderer  = nullptr;
    Time*            time      = nullptr;
    ResourceManager* resources = nullptr;
    IInput*          input     = nullptr;
    CameraManager*   cameras   = nullptr;
    vfs::Vfs*        vfs       = nullptr;
    /// デバッグプリミティブ描画。常に存在する（WITCH_DEBUG_DRAW OFF では全メソッド no-op）。
    debug::DebugDraw* debugDraw = nullptr;
    /// オーディオ。デバイス初期化に失敗した環境では nullptr のまま（無音で続行）。
    /// 呼び出し側は他サービスと同様に null チェックする。
    audio::IAudio* audio = nullptr;

    static Services& Instance() {
        static Services s;
        return s;
    }

private:
    Services() = default;
};

} // namespace witch
