#include "WitchEngine/Graphics2D/SpriteComponent.h"
#include "WitchEngine/Graphics2D/Camera2D.h"
#include "WitchEngine/Graphics2D/CameraManager.h"
#include "WitchEngine/Scene/GameObject.h"
#include "WitchEngine/Core/Services.h"
#include "WitchEngine/Rhi/IRenderer.h"
#ifdef WITCH_DEBUG_UI
#include <imgui.h>
#endif

namespace witch {

SpriteComponent::SpriteComponent(const TextureInfo& texture, float width, float height,
                                 Anchor anchor)
    : texture_(texture), width_(width), height_(height), anchor_(anchor) {}

void SpriteComponent::SetSourceRect(int x, int y, int width, int height) {
    if (texture_.width <= 0 || texture_.height <= 0) return;
    const float tw = static_cast<float>(texture_.width);
    const float th = static_cast<float>(texture_.height);
    u0_ = static_cast<float>(x) / tw;
    v0_ = static_cast<float>(y) / th;
    u1_ = static_cast<float>(x + width) / tw;
    v1_ = static_cast<float>(y + height) / th;
}

void SpriteComponent::ClearSourceRect() {
    u0_ = 0.0f; v0_ = 0.0f; u1_ = 1.0f; v1_ = 1.0f;
}

uint32_t SpriteComponent::SortKey() const {
    // int16_t を 0x8000 バイアスで昇順の uint16_t にしてから bits 8..23 に置く。
    // bit 24 は空間ビット: Screen (HUD) は常に World 全体の手前。
    const uint32_t spaceBit = space_ == SpriteSpace::Screen ? (1u << 24) : 0u;
    return spaceBit
         | (static_cast<uint32_t>(static_cast<uint16_t>(layer_ + 0x8000)) << 8);
}

void SpriteComponent::Update([[maybe_unused]] float dt) {
    auto* renderer = Services::Instance().renderer;
    if (!renderer || !texture_.IsValid()) return;

    const Transform& t = Owner()->transform;

    // アンカー係数（0/0.5/1）。transform は描画矩形の左上ワールド座標。
    // アンカー点はその矩形内の基準点で、ズーム・回転時にこの点が固定される。
    const float fx = AnchorFactorX(anchor_);
    const float fy = AnchorFactorY(anchor_);

    // アンカー点のワールド座標。
    const float anchorWorldX = t.x + width_  * fx;
    const float anchorWorldY = t.y + height_ * fy;

    // カメラ変換は CPU 側でここで適用する（RHI/HLSL はスクリーン座標のまま）。
    // アクティブカメラを CameraManager サービスから引く。未設定ならワールド座標を素通し。
    // Screen 空間 (HUD) は transform を仮想スクリーン座標として直接使い、カメラを見ない。
    float anchorScreenX = anchorWorldX;
    float anchorScreenY = anchorWorldY;
    float drawW = width_;
    float drawH = height_;
    if (space_ == SpriteSpace::World) {
        if (CameraManager* cameras = Services::Instance().cameras) {
            const Camera2D& cam = cameras->Active();
            anchorScreenX = cam.WorldToScreenX(anchorWorldX);
            anchorScreenY = cam.WorldToScreenY(anchorWorldY);
            drawW = width_  * cam.Zoom();
            drawH = height_ * cam.Zoom();
        }
    }

    // flip は UV スワップだけで実現する（ソース矩形指定とも独立に合成できる）。
    const float u0 = flipX_ ? u1_ : u0_;
    const float u1 = flipX_ ? u0_ : u1_;
    const float v0 = flipY_ ? v1_ : v0_;
    const float v1 = flipY_ ? v0_ : v1_;

    // SubmitSprite が要求する左上スクリーン座標 = アンカースクリーン座標 − 矩形×アンカー係数。
    // これによりアンカー点を固定したまま drawW/H で拡縮される（既定 Center なら中心固定）。
    // 回転ピボットもアンカー点に一致させる（pivotX/Y = アンカー係数）。
    renderer->SubmitSprite({
        .texture  = texture_.handle,
        .x        = anchorScreenX - drawW * fx,
        .y        = anchorScreenY - drawH * fy,
        .width    = drawW,
        .height   = drawH,
        .u0 = u0, .v0 = v0, .u1 = u1, .v1 = v1,
        .color    = color_,
        .rotation = t.rotation,
        .pivotX   = fx,
        .pivotY   = fy,
        .sortKey  = SortKey(),
    });
}

#ifdef WITCH_DEBUG_UI
void SpriteComponent::DrawDebugUI() {
    ImGui::PushID(this);
    if (ImGui::TreeNode("SpriteComponent")) {
        ImGui::Text("texture: id=%u (%dx%d)", texture_.handle.id,
                    texture_.width, texture_.height);
        ImGui::Text("uv: (%.3f, %.3f)-(%.3f, %.3f)", u0_, v0_, u1_, v1_);
        int layer = layer_;
        if (ImGui::DragInt("layer", &layer, 1.0f, -32768, 32767))
            layer_ = static_cast<int16_t>(layer);
        ImGui::ColorEdit4("color", &color_.r);
        ImGui::Checkbox("flipX", &flipX_);
        ImGui::SameLine();
        ImGui::Checkbox("flipY", &flipY_);
        ImGui::TreePop();
    }
    ImGui::PopID();
}
#endif

} // namespace witch
