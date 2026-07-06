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

    static Services& Instance() {
        static Services s;
        return s;
    }

private:
    Services() = default;
};

} // namespace witch
