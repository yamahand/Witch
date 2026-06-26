#pragma once
#include "WitchEngine/Rhi/RhiTypes.h"
#include <cstdint>
#include <expected>
#include <string>

namespace witch::rhi {

class ICommandList {
public:
    virtual ~ICommandList() = default;

    virtual void Clear(const ClearDesc& desc) = 0;

    // Flushes all sprites submitted via IRenderer::SubmitSprite into this command list.
    virtual void FlushSprites() = 0;
};

class IRenderer {
public:
    virtual ~IRenderer() = default;

    // windowHandle is HWND cast to void* on Win32; implementation casts back internally.
    virtual bool Init(void* windowHandle, int width, int height) = 0;

    virtual ICommandList* BeginFrame() = 0;
    virtual void EndFrame(ICommandList* cmdList) = 0;
    virtual void OnResize(int width, int height) = 0;
    virtual void Shutdown() = 0;

    // pixels must be RGBA (4 bytes/pixel). Returns a handle valid until DestroyTexture.
    virtual std::expected<TextureHandle, std::string> CreateTexture(
        const uint8_t* pixels, int width, int height) = 0;

    virtual void DestroyTexture(TextureHandle handle) = 0;

    // Queues a sprite for drawing this frame. Flushed by ICommandList::FlushSprites.
    virtual void SubmitSprite(const SpriteDrawDesc& desc) = 0;
};

} // namespace witch::rhi
