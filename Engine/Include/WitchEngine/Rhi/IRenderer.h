#pragma once
#include "WitchEngine/Rhi/RhiTypes.h"
#include <cstdint>
#include <expected>
#include <string>

namespace witch::rhi {

/// 1 フレーム分の描画コマンドを記録するインターフェース。
class ICommandList {
public:
    virtual ~ICommandList() = default;

    /// バックバッファを指定色でクリアする。
    virtual void Clear(const ClearDesc& desc) = 0;

    /// IRenderer::SubmitSprite で蓄積したスプライトを描画コマンドに変換する。
    virtual void FlushSprites() = 0;

#ifdef WITCH_DEBUG_DRAW
    /// IRenderer::SubmitLine で蓄積したデバッグ線分を描画コマンドに変換する。
    /// FlushSprites の後（= 全スプライトの手前）・RenderDebugUI の前に呼ぶ。
    virtual void FlushLines() {}
#endif
};

/// RHI 抽象インターフェース。D3D12 等の実装詳細をこの境界で完全に隠蔽する。
class IRenderer {
public:
    virtual ~IRenderer() = default;

    /// @param windowHandle Win32 では HWND を void* にキャストして渡す。実装内部でキャストし直す。
    virtual bool Init(void* windowHandle, int width, int height) = 0;

    /// フレーム開始。コマンドリストを返す。
    virtual ICommandList* BeginFrame() = 0;
    /// コマンドリストを実行し、スワップチェーンを Present する。
    virtual void EndFrame(ICommandList* cmdList) = 0;
    virtual void OnResize(int width, int height) = 0;
    virtual void Shutdown() = 0;

    /// 現在の描画先（バックバッファ）サイズをピクセルで返す。
    /// カメラのビューポート同期などに使う。D3D12 型は一切漏らさない。
    virtual int Width() const = 0;
    virtual int Height() const = 0;

    // ── 仮想解像度（固定視界）─────────────────────────────────────────────
    // SubmitSprite に渡す座標系を「仮想解像度」に固定し、ウィンドウへは一様
    // スケール + 中央寄せ（レターボックス）で写像する。描画自体はネイティブ
    // 解像度のまま行う（低解像度 RT は作らない）。

    /// 仮想解像度を設定する。(0,0) で無効 = 従来どおりウィンドウ実サイズ座標。
    virtual void SetVirtualResolution(int width, int height) = 0;
    /// 仮想解像度が有効ならその値、無効なら Width()/Height() を返す。
    /// カメラのビューポートはこちらに同期する。
    virtual int VirtualWidth() const = 0;
    virtual int VirtualHeight() const = 0;
    /// ウィンドウクライアント座標 → 仮想座標（マウス変換用）。無効時は恒等写像。
    virtual float WindowToVirtualX(float x) const = 0;
    virtual float WindowToVirtualY(float y) const = 0;

    /// @param pixels RGBA 4バイト/ピクセルのピクセルデータ。
    /// @return DestroyTexture が呼ばれるまで有効なハンドル。失敗時はエラーメッセージ。
    virtual std::expected<TextureHandle, std::string> CreateTexture(
        const uint8_t* pixels, int width, int height) = 0;

    virtual void DestroyTexture(TextureHandle handle) = 0;

    /// スプライトを描画キューに積む。ICommandList::FlushSprites で実際のコマンドに変換される。
    virtual void SubmitSprite(const SpriteDrawDesc& desc) = 0;

    /// World 空間スプライトに適用するビュー変換を設定する。
    /// screen = world * scale + offset（一様スケール + 平行移動、単位は仮想スクリーンピクセル）。
    /// 既定は恒等（scale=1, offset=0）。Screen 空間スプライトには適用されない。
    /// 変換は FlushSprites 時に頂点シェーダで適用されるため、そのフレームの
    /// SubmitSprite の前後どちらで呼んでもよい。フレームを跨いで持続するステート。
    virtual void SetCamera(float scale, float offsetX, float offsetY) = 0;

#ifdef WITCH_DEBUG_DRAW
    /// デバッグ線分を描画キューに積む。ICommandList::FlushLines で実際のコマンドに変換される。
    /// カメラ変換の扱いは SubmitSprite と同じ（World のみ SetCamera を受ける）。
    virtual void SubmitLine(const LineDrawDesc& desc) { (void)desc; }
#endif

#ifdef WITCH_DEBUG_UI
    /// デバッグ UI のフレームを開始する。入力ポンプ後・BeginFrame の前に呼ぶ
    /// （実際の呼び出し位置は Engine::Run 参照）。
    virtual void BeginDebugUI() {}
    /// デバッグ UI の描画コマンドを記録する。FlushSprites の直後、EndFrame の前に呼ぶ。
    /// 実装は BeginFrame で開始した内部のコマンドリストに記録する（引数で渡さない）。
    virtual void RenderDebugUI() {}
#endif
};

} // namespace witch::rhi
