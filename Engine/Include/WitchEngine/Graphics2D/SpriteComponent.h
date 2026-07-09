#pragma once
#include "WitchEngine/Scene/Component.h"
#include "WitchEngine/Core/TextureInfo.h"
#include "WitchEngine/Graphics2D/Anchor.h"
#include "WitchEngine/Rhi/RhiTypes.h"
#include <cstdint>

namespace witch {

/// スプライトの座標空間（rhi::SpriteSpace の別名）。
/// World  = transform をワールド座標として扱い、カメラ変換を受ける（既定）。
/// Screen = transform を仮想スクリーン座標として直接使う。HUD 用。
///          カメラ位置・ズームの影響を受けず、常に World の手前に描かれる。
using SpriteSpace = rhi::SpriteSpace;

/// テクスチャを毎フレーム Renderer に送るコンポーネント。
class SpriteComponent : public Component {
public:
    WITCH_COMPONENT(SpriteComponent, Component);

    /// @param texture ResourceManager::LoadTexture が返すテクスチャ情報。
    ///                ピクセル指定のソース矩形の UV 換算にテクスチャ実サイズを使う。
    /// @param width/height 描画サイズ（ワールド単位）
    /// @param anchor 描画・ズーム・回転の基準点（既定は中心）。
    SpriteComponent(const TextureInfo& texture, float width, float height,
                    Anchor anchor = Anchor::Center);

    /// オーナーの Transform を読んで SubmitSprite を呼ぶ。
    void Update(float dt) override;

    /// 描画提出は Render フェーズ（Animation / Camera で確定した状態を提出する）。
    UpdatePhase Phase() const override { return UpdatePhase::Render; }

#ifdef WITCH_DEBUG_UI
    /// レイヤー・カラー・flip・ソース矩形の状態表示と編集。
    void DrawInspector() override;
#endif

    void SetAnchor(Anchor anchor) { anchor_ = anchor; }
    Anchor GetAnchor() const { return anchor_; }

    /// テクスチャを差し替える（AsepriteComponent のシート切替等）。
    /// UV は変更しないため、必要なら続けて SetSourceRect / ClearSourceRect を呼ぶ。
    void SetTexture(const TextureInfo& texture) { texture_ = texture; }
    const TextureInfo& GetTexture() const { return texture_; }

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

    /// 描画レイヤー。大きいほど手前。同一フェーズ内の GameObject 間の実行順は
    /// 未規定（ComponentScheduler.h の並列化契約）のため、同レイヤー内の前後関係を
    /// 提出順に依存してはならない。前後関係が必要な場合は必ず異なる Layer を使うこと。
    void SetLayer(int16_t layer) { layer_ = layer; }
    int16_t Layer() const { return layer_; }

    /// 座標空間（World / Screen）。Screen はカメラ変換を受けない HUD 用。
    void SetSpace(SpriteSpace space) { space_ = space; }
    SpriteSpace Space() const { return space_; }

private:
    /// レイヤーを RHI の sortKey（同一 space 内の順序キー）に変換する。
    /// 空間は SpriteDrawDesc.space で渡し、RHI が space 主・sortKey 副でソートする。
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
    SpriteSpace space_ = SpriteSpace::World;
};

} // namespace witch
