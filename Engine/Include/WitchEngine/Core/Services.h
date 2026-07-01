#pragma once

namespace witch::rhi {
class IRenderer;
} // namespace witch::rhi

namespace witch {

class Time;
class ResourceManager;
class IInput;

/// エンジンが所有するサービスへの非所有ビュー。
/// Engine が Init で各ポインタを設定し、Shutdown でクリアする。
/// サービスを直接 Singleton にせずここで一元管理することで、生成・破棄順を Engine が握る。
struct Services {
    rhi::IRenderer*  renderer  = nullptr;
    Time*            time      = nullptr;
    ResourceManager* resources = nullptr;
    IInput*          input     = nullptr;

    static Services& Instance() {
        static Services s;
        return s;
    }

private:
    Services() = default;
};

} // namespace witch
