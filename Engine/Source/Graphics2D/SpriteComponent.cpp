#include "WitchEngine/Graphics2D/SpriteComponent.h"
#include "WitchEngine/Scene/GameObject.h"
#include "WitchEngine/Core/Services.h"
#include "WitchEngine/Rhi/IRenderer.h"

namespace witch {

SpriteComponent::SpriteComponent(rhi::TextureHandle texture, float width, float height)
    : texture_(texture), width_(width), height_(height) {}

void SpriteComponent::Update([[maybe_unused]] float dt) {
    auto* renderer = Services::Instance().renderer;
    if (!renderer || !texture_.IsValid()) return;

    const Transform& t = Owner()->transform;
    renderer->SubmitSprite({
        .texture = texture_,
        .x       = t.x,
        .y       = t.y,
        .width   = width_,
        .height  = height_,
    });
}

} // namespace witch
