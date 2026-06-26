#pragma once
#include "WitchEngine/Scene/Component.h"
#include "WitchEngine/Rhi/RhiTypes.h"

namespace witch {

/// テクスチャを毎フレーム Renderer に送るコンポーネント。
class SpriteComponent : public Component {
public:
    SpriteComponent(rhi::TextureHandle texture, float width, float height);

    /// オーナーの Transform を読んで SubmitSprite を呼ぶ。
    void Update(float dt) override;

private:
    rhi::TextureHandle texture_;
    float width_;
    float height_;
};

} // namespace witch
