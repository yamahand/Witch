#include "Components/PlayerControllerComponent.h"
#include "WitchEngine/Core/Services.h"
#include "WitchEngine/Graphics2D/SpriteComponent.h"
#include "WitchEngine/Input/IInput.h"
#include "WitchEngine/Physics2D/CollisionComponent.h"
#include "WitchEngine/Scene/GameObject.h"
#include <algorithm>
#ifdef WITCH_DEBUG_UI
#include <imgui.h>
#endif

namespace witch {

void PlayerControllerComponent::OnAttach() {
    collision_ = Owner()->GetComponent<CollisionComponent>();
    sprite_ = Owner()->GetComponent<SpriteComponent>();  // 無くても動く（向き反転のみ）
}

void PlayerControllerComponent::Update(float dt) {
    IInput* input = Services::Instance().input;
    if (input == nullptr || collision_ == nullptr) {
        return;
    }

    float vx = collision_->VelocityX();
    float vy = collision_->VelocityY();
    const bool onGround = collision_->OnGround();

    // ── 左右移動: 目標速度へ加速度で漸近（地上と空中で応答を変える） ──
    float dir = 0.0f;
    if (input->IsDown(Key::Left)) {
        dir -= 1.0f;
    }
    if (input->IsDown(Key::Right)) {
        dir += 1.0f;
    }
    const float accel = onGround ? groundAccel_ : airAccel_;
    const float target = dir * moveSpeed_;
    if (vx < target) {
        vx = std::min(vx + accel * dt, target);
    } else if (vx > target) {
        vx = std::max(vx - accel * dt, target);
    }
    if (dir != 0.0f && sprite_ != nullptr) {
        sprite_->SetFlip(dir < 0.0f, false);
    }

    // ── 重力（y-down なので +Y へ加速、終端速度でクランプ） ──
    vy = std::min(vy + gravity_ * dt, maxFallSpeed_);

    // ── ジャンプ: IsDown の自前エッジ検出（ヘッダの prevJumpHeld_ コメント参照） ──
    const bool jumpHeld = input->IsDown(Key::Z);
    const bool jumpPressed = jumpHeld && !prevJumpHeld_;
    const bool jumpReleased = !jumpHeld && prevJumpHeld_;
    prevJumpHeld_ = jumpHeld;

    if (jumpPressed && onGround) {
        vy = jumpSpeed_;
    }
    // 可変ジャンプ高: 上昇中に離したら減衰（離した瞬間の 1 回だけ）。
    if (jumpReleased && vy < 0.0f) {
        vy *= jumpCutFactor_;
    }

    collision_->SetVelocity(vx, vy);
}

#ifdef WITCH_DEBUG_UI
void PlayerControllerComponent::DrawInspector() {
    if (collision_ != nullptr) {
        ImGui::Text("vel: (%.1f, %.1f) onGround=%d", collision_->VelocityX(),
                    collision_->VelocityY(), collision_->OnGround() ? 1 : 0);
    }
    ImGui::DragFloat("moveSpeed", &moveSpeed_, 1.0f, 0.0f, 400.0f);
    ImGui::DragFloat("groundAccel", &groundAccel_, 10.0f, 0.0f, 4000.0f);
    ImGui::DragFloat("airAccel", &airAccel_, 10.0f, 0.0f, 4000.0f);
    ImGui::DragFloat("gravity", &gravity_, 10.0f, 0.0f, 4000.0f);
    ImGui::DragFloat("maxFallSpeed", &maxFallSpeed_, 1.0f, 0.0f, 1000.0f);
    ImGui::DragFloat("jumpSpeed", &jumpSpeed_, 1.0f, -600.0f, 0.0f);
    ImGui::DragFloat("jumpCutFactor", &jumpCutFactor_, 0.01f, 0.0f, 1.0f);
}
#endif

} // namespace witch
