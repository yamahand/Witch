#pragma once
#include "WitchEngine/Scene/Component.h"
#include "WitchEngine/Rhi/RhiTypes.h"

namespace witch {

class SpriteComponent : public Component {
public:
    SpriteComponent(rhi::TextureHandle texture, float width, float height);

    void Update(float dt) override;

private:
    rhi::TextureHandle texture_;
    float width_;
    float height_;
};

} // namespace witch
