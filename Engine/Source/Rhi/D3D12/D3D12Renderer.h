#pragma once
#define WIN32_LEAN_AND_MEAN
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstdint>
#include "WitchEngine/Rhi/IRenderer.h"
#include "Rhi/D3D12/D3D12CommandList.h"

namespace witch {

using Microsoft::WRL::ComPtr;

static constexpr uint32_t kBackBufferCount = 2;

class D3D12Renderer : public rhi::IRenderer {
public:
    bool Init(void* windowHandle, int width, int height) override;
    rhi::ICommandList* BeginFrame() override;
    void EndFrame(rhi::ICommandList* cmdList) override;
    void OnResize(int width, int height) override;
    void Shutdown() override;

private:
    void CreateBackBufferRTVs();
    void WaitForFrame(uint32_t frameIdx);
    void WaitIdle();

    struct FrameCtx {
        ComPtr<ID3D12CommandAllocator> allocator;
        uint64_t fenceValue = 0;
    };

    ComPtr<ID3D12Device2>           device_;
    ComPtr<ID3D12CommandQueue>      queue_;
    ComPtr<IDXGISwapChain3>         swapChain_;
    ComPtr<ID3D12DescriptorHeap>    rtvHeap_;
    ComPtr<ID3D12Resource>          backBuffers_[kBackBufferCount];
    D3D12_CPU_DESCRIPTOR_HANDLE     rtvHandles_[kBackBufferCount]{};
    uint32_t                        rtvDescSize_ = 0;

    FrameCtx                        frames_[kBackBufferCount];
    ComPtr<ID3D12GraphicsCommandList> cmdList_;

    ComPtr<ID3D12Fence>             fence_;
    HANDLE                          fenceEvent_ = nullptr;
    uint64_t                        fenceCounter_ = 0;

    uint32_t                        frameIndex_ = 0;
    int                             width_ = 0;
    int                             height_ = 0;

    D3D12CommandList                cmdListWrapper_;
};

} // namespace witch
