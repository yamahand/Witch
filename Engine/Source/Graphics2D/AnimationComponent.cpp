#include "WitchEngine/Graphics2D/AnimationComponent.h"
#include "WitchEngine/Graphics2D/SpriteComponent.h"
#include "WitchEngine/Scene/GameObject.h"
#include "WitchEngine/Core/Logger.h"
#include <cmath>
#ifdef WITCH_DEBUG_UI
#include <imgui.h>
#endif

namespace witch {

AnimationComponent::AnimationComponent(AnimationClip clip)
    : clip_(std::move(clip)) {}

bool AnimationComponent::ResolveSprite() {
    if (sprite_) return true;
    // 兄弟 SpriteComponent をキャッシュする（毎フレーム GetComponent しない規約）。
    // AnimationComponent が先に AddComponent された場合は OnAttach 時点で存在しない
    // ため、最初の Update での遅延解決を許す（ヘッダの順序に関する注記参照）。
    sprite_ = Owner()->GetComponent<SpriteComponent>();
    if (sprite_) {
        ApplyFrame();
        return true;
    }
    if (!warnedNoSprite_) {
        log::Warn("AnimationComponent: owner has no SpriteComponent; staying inert.");
        warnedNoSprite_ = true;
    }
    return false;
}

void AnimationComponent::OnAttach() {
    // 同じ GameObject に既に SpriteComponent があれば即キャッシュして初期コマを反映。
    // まだ無ければ警告せず最初の Update で解決を試みる。
    if (!sprite_)
        sprite_ = Owner()->GetComponent<SpriteComponent>();
    if (sprite_)
        ApplyFrame();
}

void AnimationComponent::Update(float dt) {
    if (!ResolveSprite() || !playing_ || clip_.frames.empty() || clip_.fps <= 0.0f)
        return;

    time_ += dt;
    const int count = static_cast<int>(clip_.frames.size());
    int idx = static_cast<int>(time_ * clip_.fps);
    if (clip_.loop) {
        // 周期でラップして time_ の無制限な蓄積を防ぐ（長時間再生で float の
        // 仮数部精度が枯渇するとコマ送りがスタッターするため）。
        const float period = static_cast<float>(count) / clip_.fps;
        if (time_ >= period) {
            time_ = std::fmod(time_, period);
            idx   = static_cast<int>(time_ * clip_.fps);
        }
        idx %= count; // 端数の丸めで count ちょうどになる境界ケースの保険。
    } else if (idx >= count) {
        idx = count - 1;
        playing_  = false;
        finished_ = true;
    }
    if (idx != frameIndex_) {
        frameIndex_ = idx;
        ApplyFrame();
    }
}

void AnimationComponent::Play() {
    time_       = 0.0f;
    frameIndex_ = 0;
    playing_    = true;
    finished_   = false;
    ApplyFrame();
}

void AnimationComponent::Stop() {
    playing_ = false;
}

void AnimationComponent::SetClip(AnimationClip clip) {
    clip_ = std::move(clip);
    Play();
}

void AnimationComponent::ApplyFrame() {
    if (!sprite_ || clip_.frames.empty() || clip_.columns <= 0)
        return;
    const int cell = clip_.frames[frameIndex_];
    sprite_->SetSourceRect((cell % clip_.columns) * clip_.frameWidth,
                           (cell / clip_.columns) * clip_.frameHeight,
                           clip_.frameWidth, clip_.frameHeight);
}

#ifdef WITCH_DEBUG_UI
void AnimationComponent::DrawInspector() {
    ImGui::Text("frame %d/%d (cell %d)  %s%s",
                frameIndex_, static_cast<int>(clip_.frames.size()),
                clip_.frames.empty() ? -1 : clip_.frames[frameIndex_],
                playing_ ? "playing" : "stopped",
                finished_ ? " (finished)" : "");
    ImGui::DragFloat("fps", &clip_.fps, 0.5f, 0.0f, 120.0f);
    ImGui::Checkbox("loop", &clip_.loop);
    if (ImGui::Button("Play")) Play();
    ImGui::SameLine();
    if (ImGui::Button("Stop")) Stop();
}
#endif

} // namespace witch
