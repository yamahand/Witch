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

/// ダブルバッファリングのバックバッファ数。
static constexpr uint32_t kBackBufferCount    = 2;
/// SRV ヒープのエンジンテクスチャ用スロット数。実行時に動的拡張はしない。
static constexpr uint32_t kMaxTextures        = 64;
/// SRV ヒープ内の ImGui フォントテクスチャ用スロット（エンジンテクスチャの直後）。
static constexpr uint32_t kImGuiSrvSlot       = kMaxTextures;
/// 1 フレームのスプライト上限。超えたものは破棄される。
static constexpr uint32_t kMaxSpritesPerFrame = 1024;

/// Direct3D 12 による IRenderer 実装。
/// D3D12/DXGI 型はこのクラスの外に漏らさない（RHI 境界を守るため）。
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

    void ImGuiNewFrame() override;
    void ImGuiRender(rhi::ICommandList* cmdList) override;

    /// D3D12CommandList::FlushSprites から呼ばれる。
    /// スプライトバッチを頂点バッファに書き込んでドローコールを発行する。
    void DoFlushSprites(ID3D12GraphicsCommandList* cl);

private:
    void CreateBackBufferRTVs();
    /// 指定フレームインデックスの GPU 処理完了をフェンスで待つ。
    void WaitForFrame(uint32_t frameIdx);
    void WaitIdle();
    bool InitSpritePipeline();

    /// ダブルバッファリング用フレームコンテキスト。
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

    /// テクスチャアップロード専用コマンドリスト。cmdList_（フレーム描画用）とは完全に分離。
    ComPtr<ID3D12GraphicsCommandList> uploadCmdList_;

    uint32_t                          frameIndex_   = 0;
    int                               width_        = 0;
    int                               height_       = 0;

    D3D12CommandList                  cmdListWrapper_;

    // ── Sprite pipeline ─────────────────────────────────────────────────────────
    ComPtr<ID3D12RootSignature>       spriteRootSig_;
    ComPtr<ID3D12PipelineState>       spritePSO_;

    /// 256 バイトアライン済み定数バッファ（永続マップ）。
    static constexpr uint32_t kCBAlignedSize = 256;
    ComPtr<ID3D12Resource>            cbUpload_[kBackBufferCount];
    uint8_t*                          cbMapped_[kBackBufferCount]{};

    /// フレーム毎動的頂点バッファ（永続マップ）。
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

    /// このフレームで蓄積したスプライト。DoFlushSprites でクリアされる。
    std::vector<rhi::SpriteDrawDesc>  pendingSprites_;
};

} // namespace witch
