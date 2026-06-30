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

    /// @param pixels RGBA 4バイト/ピクセルのピクセルデータ。
    /// @return DestroyTexture が呼ばれるまで有効なハンドル。失敗時はエラーメッセージ。
    virtual std::expected<TextureHandle, std::string> CreateTexture(
        const uint8_t* pixels, int width, int height) = 0;

    virtual void DestroyTexture(TextureHandle handle) = 0;

    /// スプライトを描画キューに積む。ICommandList::FlushSprites で実際のコマンドに変換される。
    virtual void SubmitSprite(const SpriteDrawDesc& desc) = 0;

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
