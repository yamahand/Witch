#pragma once
#include "WitchEngine/Rhi/RhiTypes.h"

namespace witch::rhi {

// Per-frame command recording handle. Owned by IRenderer; do not delete.
class ICommandList {
public:
    virtual ~ICommandList() = default;

    virtual void Clear(const ClearDesc& desc) = 0;
    // M2 以降: DrawSprite 等を追加予定
};

class IRenderer {
public:
    virtual ~IRenderer() = default;

    // windowHandle は Win32 では HWND を void* で渡す。実装内部でキャストする。
    virtual bool Init(void* windowHandle, int width, int height) = 0;

    // フレーム開始。コマンド記録用 ICommandList を返す。
    virtual ICommandList* BeginFrame() = 0;

    // コマンドをサブミットして Present する。
    virtual void EndFrame(ICommandList* cmdList) = 0;

    // ウィンドウリサイズ通知。
    virtual void OnResize(int width, int height) = 0;

    virtual void Shutdown() = 0;
};

} // namespace witch::rhi
