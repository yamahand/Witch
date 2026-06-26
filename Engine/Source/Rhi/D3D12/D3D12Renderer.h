#pragma once
#define WIN32_LEAN_AND_MEAN
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstdint>
#include <expected>
#include <string>
#include <vector>
#include "WitchEngine/Rhi/IRenderer.h"
#include "Rhi/D3D12/D3D12CommandList.h"

namespace witch {

using Microsoft::WRL::ComPtr;

static constexpr uint32_t kBackBufferCount    = 2;
static constexpr uint32_t kMaxTextures        = 64;
static constexpr uint32_t kMaxSpritesPerFrame = 1024;

class D3D12Renderer : public rhi::IRenderer {
public:
    bool Init(void* windowHandle, int width, int height) override;
    rhi::ICommandList* BeginFrame() override;
    void EndFrame(rhi::ICommandList* cmdList) override;
    void OnResize(int width, int height) override;
    void Shutdown() override;

    std::expected<rhi::TextureHandle, std::string> CreateTexture(
        const uint8_t* pixels, int width, int height) override;
    void DestroyTexture(rhi::TextureHandle handle) override;
    void SubmitSprite(const rhi::SpriteDrawDesc& desc) override;

    // Called by D3D12CommandList::FlushSprites.
    void DoFlushSprites(ID3D12GraphicsCommandList* cl);

private:
    void CreateBackBufferRTVs();
    void WaitForFrame(uint32_t frameIdx);
    void WaitIdle();
    bool InitSpritePipeline();

    struct FrameCtx {
        ComPtr<ID3D12CommandAllocator> allocator;
        uint64_t fenceValue = 0;
    };

    // ── Core D3D12 ─────────────────────────────────────────────────────────────
    ComPtr<ID3D12Device2>             device_;
    ComPtr<ID3D12CommandQueue>        queue_;
    ComPtr<IDXGISwapChain3>           swapChain_;
    ComPtr<ID3D12DescriptorHeap>      rtvHeap_;
    ComPtr<ID3D12Resource>            backBuffers_[kBackBufferCount];
    D3D12_CPU_DESCRIPTOR_HANDLE       rtvHandles_[kBackBufferCount]{};
    uint32_t                          rtvDescSize_ = 0;

    FrameCtx                          frames_[kBackBufferCount];
    ComPtr<ID3D12GraphicsCommandList> cmdList_;
    ComPtr<ID3D12CommandAllocator>    uploadAllocator_;

    ComPtr<ID3D12Fence>               fence_;
    HANDLE                            fenceEvent_   = nullptr;
    uint64_t                          fenceCounter_ = 0;

    uint32_t                          frameIndex_   = 0;
    int                               width_        = 0;
    int                               height_       = 0;

    D3D12CommandList                  cmdListWrapper_;

    // ── Sprite pipeline ─────────────────────────────────────────────────────────
    ComPtr<ID3D12RootSignature>       spriteRootSig_;
    ComPtr<ID3D12PipelineState>       spritePSO_;

    // Per-frame constant buffer (256-byte aligned, persistently mapped).
    static constexpr uint32_t kCBAlignedSize = 256;
    ComPtr<ID3D12Resource>            cbUpload_[kBackBufferCount];
    uint8_t*                          cbMapped_[kBackBufferCount]{};

    // Per-frame dynamic vertex buffer (persistently mapped).
    struct SpriteVertex { float x, y, u, v; };
    static constexpr uint32_t kVBSize =
        kMaxSpritesPerFrame * 4 * sizeof(SpriteVertex);
    ComPtr<ID3D12Resource>            vbUpload_[kBackBufferCount];
    uint8_t*                          vbMapped_[kBackBufferCount]{};

    // ── Texture management ──────────────────────────────────────────────────────
    ComPtr<ID3D12DescriptorHeap>      srvHeap_;
    uint32_t                          srvDescSize_ = 0;
    struct TextureEntry {
        ComPtr<ID3D12Resource> resource;
        bool used = false;
    };
    TextureEntry                      textures_[kMaxTextures]{};

    // Sprites accumulated this frame; cleared in DoFlushSprites.
    std::vector<rhi::SpriteDrawDesc>  pendingSprites_;
};

} // namespace witch
