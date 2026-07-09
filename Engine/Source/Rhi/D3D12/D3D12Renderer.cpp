#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Rhi/D3D12/D3D12Renderer.h"
#include "WitchEngine/Core/Logger.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <string>
#include <tuple>
#ifdef WITCH_DEBUG_UI
#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#endif

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

namespace witch {

namespace {

bool Check(HRESULT hr, std::string_view msg) {
    if (FAILED(hr)) {
        log::Error("{} (HRESULT=0x{:08X})", msg, static_cast<uint32_t>(hr));
        return false;
    }
    return true;
}

// Returns path relative to the running executable's directory.
std::wstring ExeRelativePath(const wchar_t* rel) {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p(buf);
    auto pos = p.rfind(L'\\');
    if (pos != std::wstring::npos) p.resize(pos + 1);
    return p + rel;
}

} // namespace

// ── D3D12CommandList ─────────────────────────────────────────────────────────

void D3D12CommandList::Clear(const rhi::ClearDesc& desc) {
    renderer->DoClear(raw, rtv, desc);
}

void D3D12CommandList::FlushSprites() {
    renderer->DoFlushSprites(raw);
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

    // 11. Upload command allocator + dedicated command list for texture uploads.
    //     Kept separate from cmdList_ so CreateTexture never touches the frame recording.
    if (!Check(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                               IID_PPV_ARGS(&uploadAllocator_)),
               "CreateCommandAllocator (upload)"))
        return false;
    if (!Check(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                          uploadAllocator_.Get(),
                                          nullptr, IID_PPV_ARGS(&uploadCmdList_)),
               "CreateCommandList (upload)"))
        return false;
    uploadCmdList_->Close();

    // 12. Sprite pipeline (shaders, root sig, PSO, per-frame buffers, SRV heap)
    if (!InitSpritePipeline())
        return false;

    // 13. Dear ImGui (Win32 + DX12 backend)
#ifdef WITCH_DEBUG_UI
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    if (!ImGui_ImplWin32_Init(hwnd)) {
        log::Error("ImGui_ImplWin32_Init failed.");
        ImGui::DestroyContext();
        return false;
    }

    // ImGui 1.92 以降の DX12 バックエンドは動的テクスチャ（RendererHasTextures）に対応し、
    // フォントアトラスを含むテクスチャを RenderDrawData 内で自前アップロードする。これには
    // SRV ディスクリプタの確保/解放コールバックが必要。レガシーな 6 引数 Init はこのフラグを
    // 無効化してしまい、v1.92 ではフォントアトラスが構築されず assert になる。
    // 本エンジンは ImGui テクスチャを font 1 枚しか使わないため、予約済みの kImGuiSrvSlot を
    // 単一スロットとして払い出す最小アロケータを与える。
    ImGui_ImplDX12_InitInfo initInfo{};
    initInfo.Device            = device_.Get();
    initInfo.CommandQueue      = queue_.Get();
    initInfo.NumFramesInFlight = static_cast<int>(kBackBufferCount);
    initInfo.RTVFormat         = DXGI_FORMAT_R8G8B8A8_UNORM;
    initInfo.SrvDescriptorHeap = srvHeap_.Get();
    initInfo.UserData          = this;
    initInfo.SrvDescriptorAllocFn =
        [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* outCpu,
           D3D12_GPU_DESCRIPTOR_HANDLE* outGpu) {
            auto* self = static_cast<D3D12Renderer*>(info->UserData);
            assert(!self->imguiSrvAllocated_ &&
                   "ImGui SRV slot allocated twice "
                   "(kImGuiSrvSlot は font 1 枚専用。ImGui::Image 等で複数テクスチャを使うなら "
                   "可変長アロケータへ拡張すること)");
            self->imguiSrvAllocated_ = true;
            *outCpu = self->srvHeap_->GetCPUDescriptorHandleForHeapStart();
            outCpu->ptr += kImGuiSrvSlot * self->srvDescSize_;
            *outGpu = self->srvHeap_->GetGPUDescriptorHandleForHeapStart();
            outGpu->ptr += kImGuiSrvSlot * self->srvDescSize_;
        };
    initInfo.SrvDescriptorFreeFn =
        [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE,
           D3D12_GPU_DESCRIPTOR_HANDLE) {
            // 単一の予約スロットのため実際の解放処理は不要。ただし将来フォントを作り直す等で
            // Free→Alloc が再発生したとき AllocFn の guard が誤発火しないようフラグを戻す。
            auto* self = static_cast<D3D12Renderer*>(info->UserData);
            self->imguiSrvAllocated_ = false;
        };
    if (!ImGui_ImplDX12_Init(&initInfo)) {
        log::Error("ImGui_ImplDX12_Init failed.");
        // 既に成功した Win32 バックエンドも巻き戻す。
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        return false;
    }
#endif // WITCH_DEBUG_UI

    log::Info("D3D12Renderer initialized ({}x{}).", width, height);
    return true;
}

rhi::ICommandList* D3D12Renderer::BeginFrame() {
    WaitForFrame(frameIndex_);

    frames_[frameIndex_].allocator->Reset();
    cmdList_->Reset(frames_[frameIndex_].allocator.Get(), nullptr);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = backBuffers_[frameIndex_].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList_->ResourceBarrier(1, &barrier);

    cmdList_->OMSetRenderTargets(1, &rtvHandles_[frameIndex_], FALSE, nullptr);

    cmdListWrapper_.raw      = cmdList_.Get();
    cmdListWrapper_.rtv      = rtvHandles_[frameIndex_];
    cmdListWrapper_.renderer = this;
    return &cmdListWrapper_;
}

void D3D12Renderer::EndFrame([[maybe_unused]] rhi::ICommandList* cmdList) {
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

    swapChain_->Present(1, 0);

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

    width_      = width;
    height_     = height;
    frameIndex_ = swapChain_->GetCurrentBackBufferIndex();
    CreateBackBufferRTVs();

    log::Info("D3D12Renderer resized to {}x{}.", width, height);
}

void D3D12Renderer::Shutdown() {
    // Init 失敗後にも呼ばれる（Engine が失敗パスで後始末する）ため、
    // 部分初期化状態を許容する: 未生成のものはスキップして生成済みだけ解放する。

    // GPU がフレームリソース（ImGui フォントテクスチャ含む）を使い終わるのを待ってから解放する。
    if (fence_ && fenceEvent_) {
        WaitIdle();
    }

#ifdef WITCH_DEBUG_UI
    // Init が ImGui まで到達しなかった / 失敗して巻き戻した場合はコンテキストが無い。
    // （Init 内の各失敗パスはバックエンドとコンテキストを対で巻き戻す）
    if (ImGui::GetCurrentContext()) {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
#endif

    // Unmap persistently mapped upload buffers before releasing.
    for (uint32_t i = 0; i < kBackBufferCount; ++i) {
        if (cbMapped_[i] && cbUpload_[i]) { cbUpload_[i]->Unmap(0, nullptr); cbMapped_[i] = nullptr; }
        if (vbMapped_[i] && vbUpload_[i]) { vbUpload_[i]->Unmap(0, nullptr); vbMapped_[i] = nullptr; }
    }

    if (fenceEvent_) {
        CloseHandle(fenceEvent_);
        fenceEvent_ = nullptr;
    }
    log::Info("D3D12Renderer shutdown complete.");
}

// ── IRenderer: texture + sprite ──────────────────────────────────────────────

std::expected<rhi::TextureHandle, std::string>
D3D12Renderer::CreateTexture(const uint8_t* pixels, int width, int height) {
    // Find a free texture slot (id = slot index + 1).
    uint32_t slot = kMaxTextures;
    for (uint32_t i = 0; i < kMaxTextures; ++i) {
        if (!textures_[i].used) { slot = i; break; }
    }
    if (slot == kMaxTextures)
        return std::unexpected<std::string>("Texture slot limit reached");

    // Create GPU-side texture resource (starts in COPY_DEST).
    D3D12_HEAP_PROPERTIES defaultHeap{};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC texDesc{};
    texDesc.Dimension          = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width              = static_cast<UINT64>(width);
    texDesc.Height             = static_cast<UINT>(height);
    texDesc.DepthOrArraySize   = 1;
    texDesc.MipLevels          = 1;
    texDesc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count   = 1;
    texDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags              = D3D12_RESOURCE_FLAG_NONE;

    ComPtr<ID3D12Resource> texResource;
    if (!Check(device_->CreateCommittedResource(
            &defaultHeap, D3D12_HEAP_FLAG_NONE,
            &texDesc, D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr, IID_PPV_ARGS(&texResource)),
               "CreateCommittedResource (texture)"))
        return std::unexpected<std::string>("CreateCommittedResource failed");

    // Query upload size and layout.
    UINT64 uploadSize = 0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT   numRows      = 0;
    UINT64 rowSizeBytes = 0;
    device_->GetCopyableFootprints(&texDesc, 0, 1, 0,
        &footprint, &numRows, &rowSizeBytes, &uploadSize);

    // Create upload buffer.
    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC uploadDesc{};
    uploadDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDesc.Width              = uploadSize;
    uploadDesc.Height             = 1;
    uploadDesc.DepthOrArraySize   = 1;
    uploadDesc.MipLevels          = 1;
    uploadDesc.Format             = DXGI_FORMAT_UNKNOWN;
    uploadDesc.SampleDesc.Count   = 1;
    uploadDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ComPtr<ID3D12Resource> uploadBuf;
    if (!Check(device_->CreateCommittedResource(
            &uploadHeap, D3D12_HEAP_FLAG_NONE,
            &uploadDesc, D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, IID_PPV_ARGS(&uploadBuf)),
               "CreateCommittedResource (upload)"))
        return std::unexpected<std::string>("Upload buffer creation failed");

    // Map and copy pixel data row by row (account for row pitch padding).
    void* mapped = nullptr;
    D3D12_RANGE readRange{0, 0};
    uploadBuf->Map(0, &readRange, &mapped);
    auto* dst = static_cast<uint8_t*>(mapped) + footprint.Offset;
    const uint32_t srcRowBytes = static_cast<uint32_t>(width) * 4u;
    for (UINT row = 0; row < numRows; ++row) {
        memcpy(dst + row * footprint.Footprint.RowPitch,
               pixels + row * srcRowBytes,
               srcRowBytes);
    }
    uploadBuf->Unmap(0, nullptr);

    // Record copy + transition using the dedicated upload command list.
    uploadAllocator_->Reset();
    uploadCmdList_->Reset(uploadAllocator_.Get(), nullptr);

    D3D12_TEXTURE_COPY_LOCATION copySrc{};
    copySrc.pResource        = uploadBuf.Get();
    copySrc.Type             = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    copySrc.PlacedFootprint  = footprint;

    D3D12_TEXTURE_COPY_LOCATION copyDst{};
    copyDst.pResource        = texResource.Get();
    copyDst.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    copyDst.SubresourceIndex = 0;

    uploadCmdList_->CopyTextureRegion(&copyDst, 0, 0, 0, &copySrc, nullptr);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource   = texResource.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    uploadCmdList_->ResourceBarrier(1, &barrier);

    uploadCmdList_->Close();
    ID3D12CommandList* lists[] = {uploadCmdList_.Get()};
    queue_->ExecuteCommandLists(1, lists);

    // Wait for the upload to finish before releasing uploadBuf.
    ++fenceCounter_;
    queue_->Signal(fence_.Get(), fenceCounter_);
    fence_->SetEventOnCompletion(fenceCounter_, fenceEvent_);
    WaitForSingleObject(fenceEvent_, INFINITE);

    // Create SRV in the GPU-visible SRV heap.
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = srvHeap_->GetCPUDescriptorHandleForHeapStart();
    cpuHandle.ptr += slot * srvDescSize_;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format                        = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension                 = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping       = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels           = 1;
    device_->CreateShaderResourceView(texResource.Get(), &srvDesc, cpuHandle);

    textures_[slot].resource = std::move(texResource);
    textures_[slot].used     = true;

    return rhi::TextureHandle{slot + 1};
}

void D3D12Renderer::DestroyTexture(rhi::TextureHandle handle) {
    if (!handle.IsValid()) return;
    WaitIdle();
    uint32_t slot = handle.id - 1;
    if (slot < kMaxTextures) {
        textures_[slot].resource.Reset();
        textures_[slot].used = false;
    }
}

#ifdef WITCH_DEBUG_UI
void D3D12Renderer::BeginDebugUI() {
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void D3D12Renderer::RenderDebugUI() {
    ImGui::Render();
    // 単一フレームコマンドリスト方式のため、BeginFrame で開始した内部の cmdList_ に直接記録する
    // （EndFrame と同じ流儀）。
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList_.Get());
}
#endif // WITCH_DEBUG_UI

void D3D12Renderer::SubmitSprite(const rhi::SpriteDrawDesc& desc) {
    if (pendingSprites_.size() < kMaxSpritesPerFrame) {
        pendingSprites_.push_back(desc);
    } else {
        static bool warnedOnce = false;
        if (!warnedOnce) {
            log::Warn("SubmitSprite: kMaxSpritesPerFrame ({}) exceeded; sprites dropped.", kMaxSpritesPerFrame);
            warnedOnce = true;
        }
    }
}

D3D12Renderer::Letterbox D3D12Renderer::ComputeLetterbox() const {
    Letterbox lb;
    if (virtualWidth_ <= 0 || virtualHeight_ <= 0 || width_ <= 0 || height_ <= 0) {
        // 仮想解像度無効 or ウィンドウ最小化: 等倍全面。
        lb.innerW = width_;
        lb.innerH = height_;
        return lb;
    }
    const float scaleX = static_cast<float>(width_)  / static_cast<float>(virtualWidth_);
    const float scaleY = static_cast<float>(height_) / static_cast<float>(virtualHeight_);
    lb.scale   = scaleX < scaleY ? scaleX : scaleY;
    lb.innerW  = static_cast<int>(virtualWidth_  * lb.scale + 0.5f);
    lb.innerH  = static_cast<int>(virtualHeight_ * lb.scale + 0.5f);
    lb.offsetX = (width_  - lb.innerW) / 2;
    lb.offsetY = (height_ - lb.innerH) / 2;
    return lb;
}

float D3D12Renderer::WindowToVirtualX(float x) const {
    const Letterbox lb = ComputeLetterbox();
    if (virtualWidth_ <= 0 || lb.scale <= 0.0f) return x;
    return (x - static_cast<float>(lb.offsetX)) / lb.scale;
}

float D3D12Renderer::WindowToVirtualY(float y) const {
    const Letterbox lb = ComputeLetterbox();
    if (virtualHeight_ <= 0 || lb.scale <= 0.0f) return y;
    return (y - static_cast<float>(lb.offsetY)) / lb.scale;
}

void D3D12Renderer::DoClear(ID3D12GraphicsCommandList* cl,
                            D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                            const rhi::ClearDesc& desc) {
    float color[4] = {desc.color.r, desc.color.g, desc.color.b, desc.color.a};
    if (virtualWidth_ <= 0 || virtualHeight_ <= 0) {
        cl->ClearRenderTargetView(rtv, color, 0, nullptr);
        return;
    }
    // レターボックス: 全面を黒帯色でクリアし、内側矩形だけシーン色でクリアする。
    // rect は RT ピクセル座標（仮想座標ではない）。
    const Letterbox lb = ComputeLetterbox();
    constexpr float kBarColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    cl->ClearRenderTargetView(rtv, kBarColor, 0, nullptr);
    D3D12_RECT inner{(LONG)lb.offsetX, (LONG)lb.offsetY,
                     (LONG)(lb.offsetX + lb.innerW), (LONG)(lb.offsetY + lb.innerH)};
    cl->ClearRenderTargetView(rtv, color, 1, &inner);
}

void D3D12Renderer::DoFlushSprites(ID3D12GraphicsCommandList* cl) {
    if (pendingSprites_.empty()) return;

    // Sort by (space, sortKey) before vertex generation: Screen (HUD) always
    // draws after (in front of) World, and the camera CBV switch below happens
    // at most once per flush. stable_sort keeps submission order for equal
    // keys, so default-key sprites draw exactly as before.
    std::stable_sort(pendingSprites_.begin(), pendingSprites_.end(),
                     [](const rhi::SpriteDrawDesc& a, const rhi::SpriteDrawDesc& b) {
                         return std::tie(a.space, a.sortKey) < std::tie(b.space, b.sortKey);
                     });

    // Write quad vertices for each sprite into the current frame's upload VB.
    auto* vbData = reinterpret_cast<SpriteVertex*>(vbMapped_[frameIndex_]);
    for (uint32_t i = 0; i < static_cast<uint32_t>(pendingSprites_.size()); ++i) {
        const auto& s = pendingSprites_[i];
        const auto& c = s.color;
        float l = s.x,          r = s.x + s.width;
        float t = s.y,          b = s.y + s.height;
        if (s.rotation == 0.0f) {
            // Fast path: axis-aligned rect (the overwhelmingly common case).
            vbData[i * 4 + 0] = {l, t, s.u0, s.v0, c.r, c.g, c.b, c.a};  // top-left
            vbData[i * 4 + 1] = {r, t, s.u1, s.v0, c.r, c.g, c.b, c.a};  // top-right
            vbData[i * 4 + 2] = {l, b, s.u0, s.v1, c.r, c.g, c.b, c.a};  // bottom-left
            vbData[i * 4 + 3] = {r, b, s.u1, s.v1, c.r, c.g, c.b, c.a};  // bottom-right
        } else {
            // Rotate the 4 corners around the pivot point.
            // Screen is y-down; this matrix keeps CCW-positive on screen
            // (rot = +π/2 turns "right of pivot" into "above pivot").
            // World 空間でも同じ: VS のビュー変換は一様スケール + 平行移動なので
            // 回転と可換であり、スクリーン空間で回してから変換した結果と一致する。
            const float px = s.x + s.width  * s.pivotX;
            const float py = s.y + s.height * s.pivotY;
            const float cs = std::cos(s.rotation);
            const float sn = std::sin(s.rotation);
            auto rot = [&](float x, float y, float u, float v) -> SpriteVertex {
                const float dx = x - px, dy = y - py;
                return {px + cs * dx + sn * dy, py - sn * dx + cs * dy,
                        u, v, c.r, c.g, c.b, c.a};
            };
            vbData[i * 4 + 0] = rot(l, t, s.u0, s.v0);  // top-left
            vbData[i * 4 + 1] = rot(r, t, s.u1, s.v0);  // top-right
            vbData[i * 4 + 2] = rot(l, b, s.u0, s.v1);  // bottom-left
            vbData[i * 4 + 3] = rot(r, b, s.u1, s.v1);  // bottom-right
        }
    }

    // Write frame constants (2 regions).
    // 仮想解像度有効時は CB に仮想サイズを書き、ビューポートをレターボックス
    // 内側矩形に絞る。VS の スクリーン→NDC 変換がそのまま仮想→内側矩形の
    // 写像になるため、シェーダ変更なしで一様スケールが成立する。
    // リージョン 0 = World（SetCamera のビュー変換）、リージョン 1 = Screen（恒等）。
    // screenSize は両リージョンに必要（NDC 変換は space によらず同じ）。
    const Letterbox lb = ComputeLetterbox();
    struct FrameCB { float screenW, screenH; float camScaleX, camScaleY;
                     float camOffsetX, camOffsetY; float pad[2]; };
    const float vw = (float)VirtualWidth(), vh = (float)VirtualHeight();
    *reinterpret_cast<FrameCB*>(cbMapped_[frameIndex_]) =
        {vw, vh, camScale_, camScale_, camOffsetX_, camOffsetY_, 0.0f, 0.0f};
    *reinterpret_cast<FrameCB*>(cbMapped_[frameIndex_] + kCBAlignedSize) =
        {vw, vh, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f};

    // Set up pipeline state.
    cl->SetPipelineState(spritePSO_.Get());
    cl->SetGraphicsRootSignature(spriteRootSig_.Get());

    D3D12_VIEWPORT vp{(float)lb.offsetX, (float)lb.offsetY,
                      (float)lb.innerW, (float)lb.innerH, 0.0f, 1.0f};
    D3D12_RECT scissor{(LONG)lb.offsetX, (LONG)lb.offsetY,
                       (LONG)(lb.offsetX + lb.innerW), (LONG)(lb.offsetY + lb.innerH)};
    cl->RSSetViewports(1, &vp);
    cl->RSSetScissorRects(1, &scissor);

    D3D12_VERTEX_BUFFER_VIEW vbv{};
    vbv.BufferLocation = vbUpload_[frameIndex_]->GetGPUVirtualAddress();
    vbv.SizeInBytes    = kVBSize;
    vbv.StrideInBytes  = sizeof(SpriteVertex);
    cl->IASetVertexBuffers(0, 1, &vbv);
    cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    ID3D12DescriptorHeap* heaps[] = {srvHeap_.Get()};
    cl->SetDescriptorHeaps(1, heaps);

    // One draw call per sprite (separate triangle strip, no degenerate-strip tricks needed).
    // Root CBV は直前のスプライトと space が変わったときだけ差し替える。
    // (space, sortKey) ソート済みなので実際の切り替えは高々 1 回だが、
    // ソート順に依存しない書き方にしておく（順序が崩れても描画は正しいまま）。
    const D3D12_GPU_VIRTUAL_ADDRESS cbBase = cbUpload_[frameIndex_]->GetGPUVirtualAddress();
    auto cbRegion = [cbBase](rhi::SpriteSpace space) {
        return space == rhi::SpriteSpace::Screen ? cbBase + kCBAlignedSize : cbBase;
    };
    cl->SetGraphicsRootConstantBufferView(0, cbRegion(pendingSprites_[0].space));
    rhi::SpriteSpace boundSpace = pendingSprites_[0].space;
    for (uint32_t i = 0; i < static_cast<uint32_t>(pendingSprites_.size()); ++i) {
        if (pendingSprites_[i].space != boundSpace) {
            boundSpace = pendingSprites_[i].space;
            cl->SetGraphicsRootConstantBufferView(0, cbRegion(boundSpace));
        }
        uint32_t texSlot = pendingSprites_[i].texture.id - 1;
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = srvHeap_->GetGPUDescriptorHandleForHeapStart();
        gpuHandle.ptr += texSlot * srvDescSize_;
        cl->SetGraphicsRootDescriptorTable(1, gpuHandle);
        cl->DrawInstanced(4, 1, i * 4, 0);
    }

    pendingSprites_.clear();
}

// ── Private helpers ──────────────────────────────────────────────────────────

void D3D12Renderer::CreateBackBufferRTVs() {
    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    for (uint32_t i = 0; i < kBackBufferCount; ++i) {
        swapChain_->GetBuffer(i, IID_PPV_ARGS(&backBuffers_[i]));
        device_->CreateRenderTargetView(backBuffers_[i].Get(), nullptr, handle);
        rtvHandles_[i]  = handle;
        handle.ptr     += rtvDescSize_;
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

bool D3D12Renderer::InitSpritePipeline() {
    // ── Compile HLSL shaders ─────────────────────────────────────────────────
    UINT compileFlags = 0;
#ifdef _DEBUG
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    auto shaderPath = ExeRelativePath(L"Shaders\\Sprite.hlsl");

    ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;
    HRESULT hr = D3DCompileFromFile(shaderPath.c_str(), nullptr, nullptr,
        "VSMain", "vs_5_0", compileFlags, 0, &vsBlob, &errBlob);
    if (FAILED(hr)) {
        if (errBlob)
            log::Error("Sprite VS compile error: {}",
                static_cast<const char*>(errBlob->GetBufferPointer()));
        log::Error("Failed to compile Sprite.hlsl (VS). HRESULT=0x{:08X}", static_cast<uint32_t>(hr));
        return false;
    }

    errBlob.Reset();
    hr = D3DCompileFromFile(shaderPath.c_str(), nullptr, nullptr,
        "PSMain", "ps_5_0", compileFlags, 0, &psBlob, &errBlob);
    if (FAILED(hr)) {
        if (errBlob)
            log::Error("Sprite PS compile error: {}",
                static_cast<const char*>(errBlob->GetBufferPointer()));
        log::Error("Failed to compile Sprite.hlsl (PS). HRESULT=0x{:08X}", static_cast<uint32_t>(hr));
        return false;
    }

    // ── Root signature ────────────────────────────────────────────────────────
    D3D12_DESCRIPTOR_RANGE srvRange{};
    srvRange.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srvRange.NumDescriptors                    = 1;
    srvRange.BaseShaderRegister                = 0;
    srvRange.RegisterSpace                     = 0;
    srvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER params[2]{};
    // [0] Root CBV at b0 (FrameCB: screen size + camera transform)
    params[0].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[0].Descriptor.ShaderRegister = 0;
    params[0].Descriptor.RegisterSpace  = 0;
    params[0].ShaderVisibility          = D3D12_SHADER_VISIBILITY_VERTEX;
    // [1] Descriptor table: 1 SRV at t0
    params[1].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[1].DescriptorTable.NumDescriptorRanges = 1;
    params[1].DescriptorTable.pDescriptorRanges   = &srvRange;
    params[1].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor      = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MaxLOD           = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister   = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rsDesc{};
    rsDesc.NumParameters     = 2;
    rsDesc.pParameters       = params;
    rsDesc.NumStaticSamplers = 1;
    rsDesc.pStaticSamplers   = &sampler;
    rsDesc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> sigBlob, sigErr;
    if (!Check(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                          &sigBlob, &sigErr), "SerializeRootSignature"))
        return false;
    if (!Check(device_->CreateRootSignature(0, sigBlob->GetBufferPointer(),
                                            sigBlob->GetBufferSize(),
                                            IID_PPV_ARGS(&spriteRootSig_)),
               "CreateRootSignature"))
        return false;

    // ── PSO ──────────────────────────────────────────────────────────────────
    D3D12_INPUT_ELEMENT_DESC inputElems[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0,  0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,  8,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature        = spriteRootSig_.Get();
    psoDesc.VS                    = {vsBlob->GetBufferPointer(), vsBlob->GetBufferSize()};
    psoDesc.PS                    = {psBlob->GetBufferPointer(), psBlob->GetBufferSize()};
    psoDesc.InputLayout           = {inputElems, 3};
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets      = 1;
    psoDesc.RTVFormats[0]         = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count      = 1;
    psoDesc.SampleMask            = UINT_MAX;

    psoDesc.RasterizerState.FillMode        = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode        = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;

    auto& rtBlend                 = psoDesc.BlendState.RenderTarget[0];
    rtBlend.BlendEnable           = TRUE;
    rtBlend.SrcBlend              = D3D12_BLEND_SRC_ALPHA;
    rtBlend.DestBlend             = D3D12_BLEND_INV_SRC_ALPHA;
    rtBlend.BlendOp               = D3D12_BLEND_OP_ADD;
    rtBlend.SrcBlendAlpha         = D3D12_BLEND_ONE;
    rtBlend.DestBlendAlpha        = D3D12_BLEND_INV_SRC_ALPHA;
    rtBlend.BlendOpAlpha          = D3D12_BLEND_OP_ADD;
    rtBlend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    psoDesc.DepthStencilState.DepthEnable   = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;

    if (!Check(device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&spritePSO_)),
               "CreateGraphicsPipelineState (sprite)"))
        return false;

    // ── Per-frame upload buffers (CB + VB, persistently mapped) ──────────────
    D3D12_HEAP_PROPERTIES uploadHeap{};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufDesc{};
    bufDesc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Height           = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels        = 1;
    bufDesc.Format           = DXGI_FORMAT_UNKNOWN;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    D3D12_RANGE readRange{0, 0};
    for (uint32_t i = 0; i < kBackBufferCount; ++i) {
        bufDesc.Width = kCBAlignedSize * kCBRegionCount;
        if (!Check(device_->CreateCommittedResource(
                &uploadHeap, D3D12_HEAP_FLAG_NONE, &bufDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&cbUpload_[i])),
                   "CreateCommittedResource (CB)"))
            return false;
        cbUpload_[i]->Map(0, &readRange, reinterpret_cast<void**>(&cbMapped_[i]));

        bufDesc.Width = kVBSize;
        if (!Check(device_->CreateCommittedResource(
                &uploadHeap, D3D12_HEAP_FLAG_NONE, &bufDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vbUpload_[i])),
                   "CreateCommittedResource (VB)"))
            return false;
        vbUpload_[i]->Map(0, &readRange, reinterpret_cast<void**>(&vbMapped_[i]));
    }

    // ── GPU-visible SRV descriptor heap ──────────────────────────────────────
    // エンジンテクスチャ用スロット kMaxTextures 個 + ImGui フォント用スロット 1 個（kImGuiSrvSlot）。
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{};
#ifdef WITCH_DEBUG_UI
    srvHeapDesc.NumDescriptors = kMaxTextures + 1;
#else
    srvHeapDesc.NumDescriptors = kMaxTextures;
#endif
    srvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (!Check(device_->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvHeap_)),
               "CreateDescriptorHeap(SRV)"))
        return false;
    srvDescSize_ = device_->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    log::Info("Sprite pipeline initialized.");
    return true;
}

} // namespace witch
