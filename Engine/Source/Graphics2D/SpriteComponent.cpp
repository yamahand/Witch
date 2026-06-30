#include "WitchEngine/Graphics2D/SpriteComponent.h"
#include "WitchEngine/Graphics2D/Camera2D.h"
#include "WitchEngine/Scene/GameObject.h"
#include "WitchEngine/Scene/Scene.h"
#include "WitchEngine/Core/Services.h"
#include "WitchEngine/Rhi/IRenderer.h"

namespace witch {

SpriteComponent::SpriteComponent(rhi::TextureHandle texture, float width, float height)
    : texture_(texture), width_(width), height_(height) {}

void SpriteComponent::Update([[maybe_unused]] float dt) {
    auto* renderer = Services::Instance().renderer;
    if (!renderer || !texture_.IsValid()) return;

    const Transform& t = Owner()->transform;

    // カメラ変換は CPU 側でここで適用する（RHI/HLSL はスクリーン座標のまま）。
    // owner の所属シーンからカメラを引く。シーン未設定ならワールド座標を素通し。
    float screenX = t.x;
    float screenY = t.y;
    float drawW   = width_;
    float drawH   = height_;
    if (Scene* scene = Owner()->GetScene()) {
        const Camera2D& cam = scene->Camera();
        // transform は描画矩形の左上原点。中心ではなく左上をワールド点として変換し、
        // サイズはズーム倍率でスケールする。
        screenX = cam.WorldToScreenX(t.x);
        screenY = cam.WorldToScreenY(t.y);
        drawW   = width_  * cam.Zoom();
        drawH   = height_ * cam.Zoom();
    }

    renderer->SubmitSprite({
        .texture = texture_,
        .x       = screenX,
        .y       = screenY,
        .width   = drawW,
        .height  = drawH,
    });
}

} // namespace witch
