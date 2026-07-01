#pragma once
#include "WitchEngine/Scene/Component.h"
#include "WitchEngine/Graphics2D/Anchor.h"
#include "WitchEngine/Rhi/RhiTypes.h"

namespace witch {

/// テクスチャを毎フレーム Renderer に送るコンポーネント。
class SpriteComponent : public Component {
public:
    /// @param anchor 描画・ズームの基準点（既定は中心）。ズーム時にこの点が固定される。
    SpriteComponent(rhi::TextureHandle texture, float width, float height,
                    Anchor anchor = Anchor::Center);

    /// オーナーの Transform を読んで SubmitSprite を呼ぶ。
    void Update(float dt) override;

    void SetAnchor(Anchor anchor) { anchor_ = anchor; }
    Anchor GetAnchor() const { return anchor_; }

private:
    rhi::TextureHandle texture_;
    float width_;
    float height_;
    Anchor anchor_;
};

} // namespace witch
