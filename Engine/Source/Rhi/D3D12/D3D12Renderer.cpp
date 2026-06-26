#include "Rhi/D3D12/D3D12Renderer.h"
#include "WitchEngine/Core/Logger.h"
#include <cassert>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

namespace witch {

namespace {

bool Check(HRESULT hr, std::string_view msg) {
    if (FAILED(hr)) {
        log::Error("{} (HRESULT=0x{:08X})", msg, static_cast<uint32_t>(hr));
        return false;
    }
    return true;
}

} // namespace

// ── D3D12CommandList ─────────────────────────────────────────────────────────

void D3D12CommandList::Clear(const rhi::ClearDesc& desc) {
    float color[4] = {desc.color.r, desc.color.g, desc.color.b, desc.color.a};
    raw->ClearRenderTargetView(rtv, color, 0, nullptr);
}

// ── D3D12Renderer ────────────────────────────────────────────────────────────

bool D3D12Renderer::Init(void* windowHandle, int width, int height) {
    HWND hwnd = static_cast<HWND>(windowHandle);
    width_  = width;
    height_ = height;

    // 1. Debug layer
#ifdef _DEBUG
    {
        ComPtr<ID3D12Debug1> debug;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
            debug->EnableDebugLayer();
            log::Info("D3D12 debug layer enabled.");
        }
    }
#endif

    // 2. DXGI factory
    UINT factoryFlags = 0;
#ifdef _DEBUG
    factoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif
    ComPtr<IDXGIFactory4> factory;
    if (!Check(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory)), "CreateDXGIFactory2"))
        return false;

    // 3. Device (default adapter, feature level 12.0)
    if (!Check(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device_)),
               "D3D12CreateDevice"))
        return false;

#ifdef _DEBUG
    ComPtr<ID3D12InfoQueue> infoQueue;
    if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
    }
#endif

    // 4. Direct command queue
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type  = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    if (!Check(device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue_)), "CreateCommandQueue"))
        return false;

    // 5. Swap chain (FLIP_DISCARD, double buffer)
    DXGI_SWAP_CHAIN_DESC1 scDesc{};
    scDesc.Width       = static_cast<UINT>(width);
    scDesc.Height      = static_cast<UINT>(height);
    scDesc.Format      = DXGI_FORMAT_R8G8B8A8_UNORM;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.BufferCount = kBackBufferCount;
    scDesc.SampleDesc  = {1, 0};
    scDesc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    ComPtr<IDXGISwapChain1> sc1;
    if (!Check(factory->CreateSwapChainForHwnd(queue_.Get(), hwnd, &scDesc,
                                               nullptr, nullptr, &sc1),
               "CreateSwapChainForHwnd"))
        return false;
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
    if (!Check(sc1.As(&swapChain_), "SwapChain → IDXGISwapChain3")) return false;
    frameIndex_ = swapChain_->GetCurrentBackBufferIndex();

    // 6. RTV descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.NumDescriptors = kBackBufferCount;
    rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    if (!Check(device_->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap_)),
               "CreateDescriptorHeap(RTV)"))
        return false;
    rtvDescSize_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // 7. RTVs for each back buffer
    CreateBackBufferRTVs();

    // 8. Per-frame command allocators
    for (uint32_t i = 0; i < kBackBufferCount; ++i) {
        if (!Check(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                   IID_PPV_ARGS(&frames_[i].allocator)),
                   "CreateCommandAllocator"))
            return false;
    }

    // 9. Command list (created open; close immediately so BeginFrame can reset)
    if (!Check(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                          frames_[frameIndex_].allocator.Get(),
                                          nullptr, IID_PPV_ARGS(&cmdList_)),
               "CreateCommandList"))
        return false;
    cmdList_->Close();

    // 10. Fence + event
    if (!Check(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)),
               "CreateFence"))
        return false;
    fenceEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!fenceEvent_) {
        log::Error("CreateEventW failed.");
        return false;
    }

    log::Info("D3D12Renderer initialized ({}x{}).", width, height);
    return true;
}

rhi::ICommandList* D3D12Renderer::BeginFrame() {
    // Wait until this frame slot's previous commands have completed.
    WaitForFrame(frameIndex_);

    frames_[frameIndex_].allocator->Reset();
    cmdList_->Reset(frames_[frameIndex_].allocator.Get(), nullptr);

    // Back buffer: PRESENT → RENDER_TARGET
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = backBuffers_[frameIndex_].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList_->ResourceBarrier(1, &barrier);

    cmdList_->OMSetRenderTargets(1, &rtvHandles_[frameIndex_], FALSE, nullptr);

    cmdListWrapper_.raw = cmdList_.Get();
    cmdListWrapper_.rtv = rtvHandles_[frameIndex_];
    return &cmdListWrapper_;
}

void D3D12Renderer::EndFrame([[maybe_unused]] rhi::ICommandList* cmdList) {
    // Back buffer: RENDER_TARGET → PRESENT
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = backBuffers_[frameIndex_].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList_->ResourceBarrier(1, &barrier);

    cmdList_->Close();

    ID3D12CommandList* lists[] = {cmdList_.Get()};
    queue_->ExecuteCommandLists(1, lists);

    swapChain_->Present(1, 0); // V-sync on

    ++fenceCounter_;
    queue_->Signal(fence_.Get(), fenceCounter_);
    frames_[frameIndex_].fenceValue = fenceCounter_;

    frameIndex_ = swapChain_->GetCurrentBackBufferIndex();
}

void D3D12Renderer::OnResize(int width, int height) {
    if (width_ == width && height_ == height) return;

    WaitIdle();

    for (auto& buf : backBuffers_) buf.Reset();

    HRESULT hr = swapChain_->ResizeBuffers(0,
        static_cast<UINT>(width), static_cast<UINT>(height),
        DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        log::Error("ResizeBuffers failed (0x{:08X})", static_cast<uint32_t>(hr));
        return;
    }

    width_  = width;
    height_ = height;
    frameIndex_ = swapChain_->GetCurrentBackBufferIndex();
    CreateBackBufferRTVs();

    log::Info("D3D12Renderer resized to {}x{}.", width, height);
}

void D3D12Renderer::Shutdown() {
    WaitIdle();
    if (fenceEvent_) {
        CloseHandle(fenceEvent_);
        fenceEvent_ = nullptr;
    }
    // ComPtrs release D3D12 objects in reverse declaration order on destruction.
    log::Info("D3D12Renderer shutdown complete.");
}

// ── Private helpers ──────────────────────────────────────────────────────────

void D3D12Renderer::CreateBackBufferRTVs() {
    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    for (uint32_t i = 0; i < kBackBufferCount; ++i) {
        swapChain_->GetBuffer(i, IID_PPV_ARGS(&backBuffers_[i]));
        device_->CreateRenderTargetView(backBuffers_[i].Get(), nullptr, handle);
        rtvHandles_[i] = handle;
        handle.ptr    += rtvDescSize_;
    }
}

void D3D12Renderer::WaitForFrame(uint32_t frameIdx) {
    if (fence_->GetCompletedValue() < frames_[frameIdx].fenceValue) {
        fence_->SetEventOnCompletion(frames_[frameIdx].fenceValue, fenceEvent_);
        WaitForSingleObject(fenceEvent_, INFINITE);
    }
}

void D3D12Renderer::WaitIdle() {
    for (uint32_t i = 0; i < kBackBufferCount; ++i)
        WaitForFrame(i);
}

} // namespace witch
