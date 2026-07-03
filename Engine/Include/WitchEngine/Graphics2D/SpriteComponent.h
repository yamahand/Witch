#pragma once
#include "WitchEngine/Scene/Component.h"
#include "WitchEngine/Core/TextureInfo.h"
#include "WitchEngine/Graphics2D/Anchor.h"
#include "WitchEngine/Rhi/RhiTypes.h"
#include <cstdint>

namespace witch {

/// テクスチャを毎フレーム Renderer に送るコンポーネント。
class SpriteComponent : public Component {
public:
    /// @param texture ResourceManager::LoadTexture が返すテクスチャ情報。
    ///                ピクセル指定のソース矩形の UV 換算にテクスチャ実サイズを使う。
    /// @param width/height 描画サイズ（ワールド単位）
    /// @param anchor 描画・ズーム・回転の基準点（既定は中心）。
    SpriteComponent(const TextureInfo& texture, float width, float height,
                    Anchor anchor = Anchor::Center);

    /// オーナーの Transform を読んで SubmitSprite を呼ぶ。
    void Update(float dt) override;

#ifdef WITCH_DEBUG_UI
    /// レイヤー・カラー・flip・ソース矩形の状態表示と編集。
    void DrawDebugUI() override;
#endif

    void SetAnchor(Anchor anchor) { anchor_ = anchor; }
    Anchor GetAnchor() const { return anchor_; }

    /// スプライトシート上のコマを px 矩形で指定する（左上原点）。
    /// テクスチャ実サイズで正規化して UV に変換する。
    void SetSourceRect(int x, int y, int width, int height);
    /// ソース矩形指定を解除してテクスチャ全面に戻す。
    void ClearSourceRect();

    /// 左右／上下の鏡像反転。提出時に UV をスワップするだけで RHI は関知しない。
    void SetFlip(bool horizontal, bool vertical) { flipX_ = horizontal; flipY_ = vertical; }
    bool FlipX() const { return flipX_; }
    bool FlipY() const { return flipY_; }

    /// テクセルに乗算されるカラー（tint + alpha）。既定は白 = 無変調。
    void SetColor(const rhi::Color& color) { color_ = color; }
    const rhi::Color& GetColor() const { return color_; }

    /// 描画レイヤー。大きいほど手前。同レイヤーは提出順（= Update 順）を維持。
    void SetLayer(int16_t layer) { layer_ = layer; }
    int16_t Layer() const { return layer_; }

private:
    /// レイヤーを RHI の sortKey に合成する。
    /// bits 8..23: layer（int16_t + 0x8000 バイアスで昇順化）。bits 0..7 は予約。
    uint32_t SortKey() const;

    TextureInfo texture_;
    float width_;
    float height_;
    Anchor anchor_;
    float u0_ = 0.0f, v0_ = 0.0f, u1_ = 1.0f, v1_ = 1.0f;  // SetSourceRect が更新
    bool flipX_ = false;
    bool flipY_ = false;
    rhi::Color color_{1.0f, 1.0f, 1.0f, 1.0f};
    int16_t layer_ = 0;
};

} // namespace witch
