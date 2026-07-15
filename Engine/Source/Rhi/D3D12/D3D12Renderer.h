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

/// トリプルバッファリングのバックバッファ数。
/// 3 面にすることで CPU が GPU を 1 フレーム先行して積め、vsync 時に軽いフレームで
/// 生じる待ちバブル（CPU が Present 直後のバッファ解放を待って余分な vsync 1 回分
/// ブロックする現象）を解消する。フェンス同期・per-frame リソース配列・ImGui の
/// NumFramesInFlight はすべてこの定数で回るため、値の変更だけで面数が揃う。
static constexpr uint32_t kBackBufferCount    = 3;
/// SRV ヒープのエンジンテクスチャ用スロット数。実行時に動的拡張はしない。
static constexpr uint32_t kMaxTextures        = 64;
/// SRV ヒープ内の ImGui フォントテクスチャ用スロット（エンジンテクスチャの直後）。
static constexpr uint32_t kImGuiSrvSlot       = kMaxTextures;
/// 1 フレームのスプライト上限。超えたものは破棄される。
/// タイルマップ（1 タイル = 1 スプライト提出）対応で 16384 に引き上げた。
/// 頂点バッファは 16384 * 4 頂点 * 32B = 2 MiB / バックバッファで無害
/// （RefactoringNotes §3）。超過は従来どおり 1 回だけ警告して破棄する。
static constexpr uint32_t kMaxSpritesPerFrame = 16384;
#ifdef WITCH_DEBUG_DRAW
/// 1 フレームのデバッグ線分上限。超えたものは破棄される（スプライトと同じ方針）。
/// 頂点バッファは 16384 * 2 頂点 * 24B = 768 KiB / バックバッファ。
static constexpr uint32_t kMaxLinesPerFrame = 16384;
#endif

/// Direct3D 12 による IRenderer 実装。
/// D3D12/DXGI 型はこのクラスの外に漏らさない（RHI 境界を守るため）。
class D3D12Renderer : public rhi::IRenderer {
public:
    bool Init(void* windowHandle, int width, int height) override;
    rhi::ICommandList* BeginFrame() override;
    void EndFrame(rhi::ICommandList* cmdList) override;
    void OnResize(int width, int height) override;
    void Shutdown() override;
    // ティアリング未対応環境では vsync を切れない（Present(0, ALLOW_TEARING) が使えず、
    // SyncInterval=0 でもドライバが vsync を強制することがある）。その場合は要求を
    // 無視して vsync ON のままにする。VSync() で実際の状態を確認できる。
    void SetVSync(bool enabled) override { vsync_ = enabled || !allowTearing_; }
    bool VSync() const override { return vsync_; }
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
#ifdef WITCH_DEBUG_DRAW
    void SubmitLine(const rhi::LineDrawDesc& desc) override;
#endif

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

#ifdef WITCH_DEBUG_DRAW
    /// D3D12CommandList::FlushLines から呼ばれる。
    /// デバッグ線分バッチを頂点バッファに書き込んでドローコールを発行する
    /// （World / Screen で最大 2 コール）。
    void DoFlushLines(ID3D12GraphicsCommandList* cl);
#endif

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
#ifdef WITCH_DEBUG_DRAW
    bool InitLinePipeline();
#endif
    /// FrameCB の 2 リージョン（World / Screen）を現フレームの CB へ書き込む。
    /// DoFlushSprites / DoFlushLines が共用する（同値の二重書き込みは無害）。
    void WriteFrameConstants();
    /// レターボックス内側矩形にビューポートとシザーを設定する。
    void SetLetterboxViewport(ID3D12GraphicsCommandList* cl) const;

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

    /// waitable swapchain のフレームレイテンシ待機オブジェクト。BeginFrame 先頭で待つことで
    /// Present のキュー詰まりブロックを明示待ちへ移し、Present 自体を即座に返させる。
    /// スワップチェイン所有（ResizeBuffers では作り直し不要）。Shutdown で CloseHandle する。
    HANDLE                            frameLatencyWaitable_ = nullptr;
    /// 待機オブジェクトの待ちが一度でも異常終了（タイムアウト/失敗）したら true。
    /// 以降は waitable 待機自体をスキップして fence-only 同期へ恒久フォールバック
    /// する（毎フレーム 1s ブロックを防ぐ）。同時に警告ログの 1 回制限も兼ねる。
    bool                              frameLatencyWaitFailed_ = false;
    /// Present 失敗（デバイスロスト等）の警告を 1 回だけ出すためのゲート。
    bool                              presentFailedWarned_ = false;

    /// テクスチャアップロード専用コマンドリスト。cmdList_（フレーム描画用）とは完全に分離。
    ComPtr<ID3D12GraphicsCommandList> uploadCmdList_;

    uint32_t                          frameIndex_   = 0;
    int                               width_        = 0;
    int                               height_       = 0;
    int                               virtualWidth_  = 0;  ///< 0 = 仮想解像度無効
    int                               virtualHeight_ = 0;
    bool                              vsync_        = true; ///< Present の SyncInterval（true=1, false=0）
    bool                              allowTearing_ = false; ///< DXGI がティアリングを許可するか（vsync OFF に必須）

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

#ifdef WITCH_DEBUG_DRAW
    // ── Debug line pipeline ─────────────────────────────────────────────────────
    /// テクスチャ不要のため専用ルートシグネチャ（b0 の Root CBV のみ）を持つ。
    /// FrameCB はスプライトと同じ cbUpload_ の 2 リージョンを共有する。
    ComPtr<ID3D12RootSignature>       lineRootSig_;
    ComPtr<ID3D12PipelineState>       linePSO_;

    struct LineVertex { float x, y; float r, g, b, a; };
    static constexpr uint32_t kLineVBSize =
        kMaxLinesPerFrame * 2 * sizeof(LineVertex);
    ComPtr<ID3D12Resource>            lineVbUpload_[kBackBufferCount];
    uint8_t*                          lineVbMapped_[kBackBufferCount]{};

    /// このフレームで蓄積したデバッグ線分。DoFlushLines でクリアされる。
    std::vector<rhi::LineDrawDesc>    pendingLines_;
#endif

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
