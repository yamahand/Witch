#include "WitchEngine/Graphics2D/AnimationComponent.h"
#include "WitchEngine/Graphics2D/SpriteComponent.h"
#include "WitchEngine/Scene/GameObject.h"
#include "WitchEngine/Core/Logger.h"
#ifdef WITCH_DEBUG_UI
#include <imgui.h>
#endif

namespace witch {

AnimationComponent::AnimationComponent(AnimationClip clip)
    : clip_(std::move(clip)) {}

void AnimationComponent::OnAttach() {
    // 兄弟 SpriteComponent をキャッシュする（毎フレーム GetComponent しない規約）。
    sprite_ = Owner()->GetComponent<SpriteComponent>();
    if (!sprite_) {
        log::Warn("AnimationComponent: owner has no SpriteComponent; staying inert. "
                  "Add SpriteComponent BEFORE AnimationComponent.");
        return;
    }
    ApplyFrame();
}

void AnimationComponent::Update(float dt) {
    if (!sprite_ || !playing_ || clip_.frames.empty() || clip_.fps <= 0.0f)
        return;

    time_ += dt;
    const int count = static_cast<int>(clip_.frames.size());
    int idx = static_cast<int>(time_ * clip_.fps);
    if (clip_.loop) {
        idx %= count;
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
void AnimationComponent::DrawDebugUI() {
    ImGui::PushID(this);
    if (ImGui::TreeNode("AnimationComponent")) {
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
        ImGui::TreePop();
    }
    ImGui::PopID();
}
#endif

} // namespace witch
