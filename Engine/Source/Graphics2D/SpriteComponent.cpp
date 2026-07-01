#include "WitchEngine/Graphics2D/SpriteComponent.h"
#include "WitchEngine/Graphics2D/Camera2D.h"
#include "WitchEngine/Scene/GameObject.h"
#include "WitchEngine/Scene/Scene.h"
#include "WitchEngine/Core/Services.h"
#include "WitchEngine/Rhi/IRenderer.h"

namespace witch {

SpriteComponent::SpriteComponent(rhi::TextureHandle texture, float width, float height,
                                 Anchor anchor)
    : texture_(texture), width_(width), height_(height), anchor_(anchor) {}

void SpriteComponent::Update([[maybe_unused]] float dt) {
    auto* renderer = Services::Instance().renderer;
    if (!renderer || !texture_.IsValid()) return;

    const Transform& t = Owner()->transform;

    // アンカー係数（0/0.5/1）。transform は描画矩形の左上ワールド座標。
    // アンカー点はその矩形内の基準点で、ズーム時にこの点が固定される。
    const float fx = AnchorFactorX(anchor_);
    const float fy = AnchorFactorY(anchor_);

    // アンカー点のワールド座標。
    const float anchorWorldX = t.x + width_  * fx;
    const float anchorWorldY = t.y + height_ * fy;

    // カメラ変換は CPU 側でここで適用する（RHI/HLSL はスクリーン座標のまま）。
    // owner の所属シーンからカメラを引く。シーン未設定ならワールド座標を素通し。
    float anchorScreenX = anchorWorldX;
    float anchorScreenY = anchorWorldY;
    float drawW = width_;
    float drawH = height_;
    if (Scene* scene = Owner()->GetScene()) {
        const Camera2D& cam = scene->Camera();
        anchorScreenX = cam.WorldToScreenX(anchorWorldX);
        anchorScreenY = cam.WorldToScreenY(anchorWorldY);
        drawW = width_  * cam.Zoom();
        drawH = height_ * cam.Zoom();
    }

    // SubmitSprite が要求する左上スクリーン座標 = アンカースクリーン座標 − 矩形×アンカー係数。
    // これによりアンカー点を固定したまま drawW/H で拡縮される（既定 Center なら中心固定）。
    renderer->SubmitSprite({
        .texture = texture_,
        .x       = anchorScreenX - drawW * fx,
        .y       = anchorScreenY - drawH * fy,
        .width   = drawW,
        .height  = drawH,
    });
}

} // namespace witch
