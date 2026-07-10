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
/// タイルマップ（1 タイル = 1 スプライト提出）対応で 16384 に引き上げた。
/// 頂点バッファは 16384 * 4 頂点 * 32B = 2 MiB / バックバッファで無害
/// （RefactoringNotes §3）。超過は従来どおり 1 回だけ警告して破棄する。
static constexpr uint32_t kMaxSpritesPerFrame = 16384;

/// Direct3D 12 による IRenderer 実装。
/// D3D12/DXGI 型はこのクラスの外に漏らさない（RHI 境界を守るため）。
class D3D12Renderer : public rhi::IRenderer {
public:
    bool Init(void* windowHandle, int width, int height) override;
    rhi::ICommandList* BeginFrame() override;
    void EndFrame(rhi::ICommandList* cmdList) override;
    void OnResize(int width, int height) override;
    void Shutdown() override;
    int Width() const override { return width_; }
    int Height() const override { return height_; }

    void SetVirtualResolution(int width, int height) override {
        virtualWidth_  = width;
        virtualHeight_ = height;
    }
    int VirtualWidth() const override {
        return virtualWidth_ > 0 ? virtualWidth_ : width_;
    }
    int VirtualHeight() const override {
        return virtualHeight_ > 0 ? virtualHeight_ : height_;
    }
    float WindowToVirtualX(float x) const override;
    float WindowToVirtualY(float y) const override;

    std::expected<rhi::TextureHandle, std::string> CreateTexture(
        const uint8_t* pixels, int width, int height) override;
    void DestroyTexture(rhi::TextureHandle handle) override;
    void SubmitSprite(const rhi::SpriteDrawDesc& desc) override;

    void SetCamera(float scale, float offsetX, float offsetY) override {
        camScale_   = scale;
        camOffsetX_ = offsetX;
        camOffsetY_ = offsetY;
    }

#ifdef WITCH_DEBUG_UI
    void BeginDebugUI() override;
    void RenderDebugUI() override;
#endif

    /// D3D12CommandList::FlushSprites から呼ばれる。
    /// スプライトバッチを頂点バッファに書き込んでドローコールを発行する。
    void DoFlushSprites(ID3D12GraphicsCommandList* cl);

    /// D3D12CommandList::Clear から呼ばれる。
    /// 仮想解像度有効時は全面黒 → レターボックス内側のみ指定色の 2 段クリア。
    void DoClear(ID3D12GraphicsCommandList* cl, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                 const rhi::ClearDesc& desc);

private:
    /// 仮想解像度→ウィンドウの一様スケール写像。毎フレーム計算する（状態を持たない）。
    struct Letterbox {
        float scale   = 1.0f;  ///< 仮想 1px がウィンドウ何 px になるか
        int   offsetX = 0;     ///< 内側矩形の左上（ウィンドウピクセル）
        int   offsetY = 0;
        int   innerW  = 0;     ///< 内側矩形サイズ（ウィンドウピクセル）
        int   innerH  = 0;
    };
    /// 仮想解像度無効・ウィンドウ最小化時は等倍全面を返す。
    Letterbox ComputeLetterbox() const;

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
    int                               virtualWidth_  = 0;  ///< 0 = 仮想解像度無効
    int                               virtualHeight_ = 0;

    D3D12CommandList                  cmdListWrapper_;

    // ── Sprite pipeline ─────────────────────────────────────────────────────────
    ComPtr<ID3D12RootSignature>       spriteRootSig_;
    ComPtr<ID3D12PipelineState>       spritePSO_;

    /// 256 バイトアライン済み定数バッファ（永続マップ）。
    /// 1 フレームあたり 2 リージョン: offset 0 = World（カメラ変換）、
    /// offset kCBAlignedSize = Screen（恒等）。DoFlushSprites が space 切替時に
    /// Root CBV を該当リージョンへ差し替える。
    static constexpr uint32_t kCBAlignedSize = 256;
    static constexpr uint32_t kCBRegionCount = 2;
    ComPtr<ID3D12Resource>            cbUpload_[kBackBufferCount];
    uint8_t*                          cbMapped_[kBackBufferCount]{};

    /// SetCamera で設定されるビュー変換（screen = world * scale + offset）。既定は恒等。
    float                             camScale_   = 1.0f;
    float                             camOffsetX_ = 0.0f;
    float                             camOffsetY_ = 0.0f;

    /// フレーム毎動的頂点バッファ（永続マップ）。
    /// カラーは頂点持ち: ルート定数だと per-sprite 状態がコマンドに焼き付き、
    /// 将来の同一テクスチャバッチングを塞ぐため。
    struct SpriteVertex { float x, y, u, v; float r, g, b, a; };
    static constexpr uint32_t kVBSize =
        kMaxSpritesPerFrame * 4 * sizeof(SpriteVertex);
    ComPtr<ID3D12Resource>            vbUpload_[kBackBufferCount];
    uint8_t*                          vbMapped_[kBackBufferCount]{};

    // ── Texture management ──────────────────────────────────────────────────────
    ComPtr<ID3D12DescriptorHeap>      srvHeap_;
    uint32_t                          srvDescSize_ = 0;
#ifdef WITCH_DEBUG_UI
    bool                              imguiSrvAllocated_ = false;  // ImGui SRV スロットの多重払い出し検出用
#endif
    struct TextureEntry {
        ComPtr<ID3D12Resource> resource;
        bool used = false;
    };
    TextureEntry                      textures_[kMaxTextures]{};

    /// このフレームで蓄積したスプライト。DoFlushSprites でクリアされる。
    std::vector<rhi::SpriteDrawDesc>  pendingSprites_;
};

} // namespace witch
