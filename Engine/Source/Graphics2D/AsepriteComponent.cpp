#include "WitchEngine/Graphics2D/AsepriteComponent.h"
#include "WitchEngine/Graphics2D/SpriteComponent.h"
#include "WitchEngine/Scene/GameObject.h"
#include "WitchEngine/Core/Logger.h"
#ifdef WITCH_DEBUG_UI
#include <imgui.h>
#endif

namespace witch {

AsepriteComponent::AsepriteComponent(std::shared_ptr<const AsepriteSheet> sheet)
    : sheet_(std::move(sheet)) {
    if (sheet_ && !sheet_->frames.empty())
        StartRange(0, static_cast<int>(sheet_->frames.size()) - 1,
                   AsepriteLoopDir::Forward, 0);
}

bool AsepriteComponent::ResolveSprite() {
    if (sprite_) return true;
    sprite_ = Owner()->GetComponent<SpriteComponent>();
    if (sprite_) {
        // 別テクスチャで作られた SpriteComponent でも動くようアトラスを張り直す。
        if (sheet_) sprite_->SetTexture(sheet_->texture);
        ApplyFrame();
        return true;
    }
    if (!warnedNoSprite_) {
        log::Warn("AsepriteComponent: owner has no SpriteComponent; staying inert.");
        warnedNoSprite_ = true;
    }
    return false;
}

void AsepriteComponent::OnAttach() {
    if (!sprite_)
        sprite_ = Owner()->GetComponent<SpriteComponent>();
    if (sprite_) {
        if (sheet_) sprite_->SetTexture(sheet_->texture);
        ApplyFrame();
    }
}

void AsepriteComponent::Update(float dt) {
    if (!ResolveSprite() || !playing_ || !sheet_ || sheet_->frames.empty())
        return;

    const int before = frame_;
    time_ += dt;
    // コマごとに duration が異なるため 1 コマずつ消化する。ロード時に duration は
    // 1ms 以上に丸めてあるが、巨大 dt での長時間ループは反復上限で打ち切る。
    int guard = 0;
    while (playing_ && guard++ < 1024) {
        const float duration = sheet_->frames[static_cast<size_t>(frame_)].duration;
        if (time_ < duration) break;
        time_ -= duration;
        Advance();
    }
    if (frame_ != before)
        ApplyFrame();
}

void AsepriteComponent::Play() {
    if (!sheet_ || sheet_->frames.empty()) return;
    StartRange(0, static_cast<int>(sheet_->frames.size()) - 1,
               AsepriteLoopDir::Forward, 0);
    ApplyFrame();
}

bool AsepriteComponent::Play(std::string_view tagName) {
    if (!sheet_) return false;
    const AsepriteTag* tag = sheet_->FindTag(tagName);
    if (!tag) {
        log::Warn("AsepriteComponent: tag \"{}\" not found.", tagName);
        return false;
    }
    StartRange(tag->from, tag->to, tag->direction, tag->repeat);
    ApplyFrame();
    return true;
}

void AsepriteComponent::Stop() {
    playing_ = false;
}

void AsepriteComponent::SetSheet(std::shared_ptr<const AsepriteSheet> sheet) {
    sheet_ = std::move(sheet);
    if (sprite_ && sheet_)
        sprite_->SetTexture(sheet_->texture);
    Play();
}

void AsepriteComponent::StartRange(int from, int to, AsepriteLoopDir dir, int repeat) {
    from_ = from;
    to_   = to;
    dir_  = dir;
    infinite_   = repeat == 0;
    repeatLeft_ = repeat;
    pingpongForward_ = dir != AsepriteLoopDir::Reverse
                    && dir != AsepriteLoopDir::PingPongReverse;
    frame_ = pingpongForward_ ? from_ : to_;
    time_  = 0.0f;
    playing_  = true;
    finished_ = false;
}

void AsepriteComponent::Advance() {
    // 単一コマの範囲: 進む先が無いので repeat の消化だけ行う。
    if (from_ == to_) {
        if (!infinite_ && --repeatLeft_ <= 0) {
            playing_  = false;
            finished_ = true;
        }
        return;
    }

    const bool pingpong = dir_ == AsepriteLoopDir::PingPong
                       || dir_ == AsepriteLoopDir::PingPongReverse;

    // 次のコマと向きを先に計算し、周回境界（= 開始端へ戻る一歩）を跨ぐ場合は
    // repeat を消化する。再生し切っていたら現在コマのまま停止する
    // （例: PingPong 0..2 repeat=1 は 0,1,2,1 で止まり、0 へは戻らない）。
    int  next        = frame_;
    bool nextForward = pingpongForward_;
    bool cycleEnd    = false;

    if (pingpongForward_) {
        if (frame_ < to_) {
            next = frame_ + 1;
        } else if (pingpong) {
            // 折り返し。端のコマは 2 連続で表示しない（Aseprite と同じ挙動）。
            nextForward = false;
            next = frame_ - 1;
        } else {
            cycleEnd = true;
            next = from_;  // Forward ループ: 先頭へ巻き戻す
        }
    } else {
        if (frame_ > from_) {
            next = frame_ - 1;
        } else if (pingpong) {
            nextForward = true;
            next = frame_ + 1;
        } else {
            cycleEnd = true;
            next = to_;    // Reverse ループ: 末尾へ巻き戻す
        }
    }

    // PingPong 系の 1 周 = 開始端に戻り着く一歩（往路 + 復路で端の重複なし）。
    if (dir_ == AsepriteLoopDir::PingPong && !nextForward && next == from_)
        cycleEnd = true;
    if (dir_ == AsepriteLoopDir::PingPongReverse && nextForward && next == to_)
        cycleEnd = true;

    if (cycleEnd && !infinite_ && --repeatLeft_ <= 0) {
        playing_  = false;
        finished_ = true;
        return;  // 最終コマの表示を保つ
    }
    frame_ = next;
    pingpongForward_ = nextForward;
}

void AsepriteComponent::ApplyFrame() {
    if (!sprite_ || !sheet_ || sheet_->frames.empty())
        return;
    const auto& f = sheet_->frames[static_cast<size_t>(frame_)];
    sprite_->SetSourceRect(f.x, f.y, sheet_->frameWidth, sheet_->frameHeight);
}

#ifdef WITCH_DEBUG_UI
void AsepriteComponent::DrawDebugUI() {
    ImGui::PushID(this);
    if (ImGui::TreeNode("AsepriteComponent")) {
        const int total = sheet_ ? static_cast<int>(sheet_->frames.size()) : 0;
        ImGui::Text("frame %d/%d  range [%d..%d]  %s%s",
                    frame_, total, from_, to_,
                    playing_ ? "playing" : "stopped",
                    finished_ ? " (finished)" : "");
        if (sheet_) {
            ImGui::Text("sheet: %dx%d, %d tags",
                        sheet_->frameWidth, sheet_->frameHeight,
                        static_cast<int>(sheet_->tags.size()));
            for (const auto& tag : sheet_->tags) {
                if (ImGui::SmallButton(tag.name.c_str()))
                    Play(tag.name);
                ImGui::SameLine();
            }
            ImGui::NewLine();
        }
        if (ImGui::Button("Play")) Play();
        ImGui::SameLine();
        if (ImGui::Button("Stop")) Stop();
        ImGui::TreePop();
    }
    ImGui::PopID();
}
#endif

} // namespace witch
