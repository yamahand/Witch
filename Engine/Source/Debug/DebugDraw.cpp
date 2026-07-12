#include "WitchEngine/Debug/DebugDraw.h"
#include "WitchEngine/Core/Logger.h"
#include "WitchEngine/Rhi/IRenderer.h"
#include <cmath>
#include <numbers>

namespace witch::debug {

DebugDraw::DebugDraw(rhi::IRenderer* renderer) : renderer_(renderer) {}

DebugDraw::~DebugDraw() {
    // OFF ビルドでは whiteTexture_ が常に無効なので何もしない。
    if (whiteTexture_.IsValid()) renderer_->DestroyTexture(whiteTexture_);
}

#ifdef WITCH_DEBUG_DRAW

// ── 提出 API ─────────────────────────────────────────────────────────────────

void DebugDraw::Line(float x0, float y0, float x1, float y1, rhi::Color color,
                     rhi::SpriteSpace space) {
    Push({Prim::Kind::Line, x0, y0, x1, y1, color, space});
}

void DebugDraw::Rect(float x, float y, float width, float height, rhi::Color color,
                     rhi::SpriteSpace space) {
    const float r = x + width, b = y + height;
    Line(x, y, r, y, color, space);
    Line(r, y, r, b, color, space);
    Line(r, b, x, b, color, space);
    Line(x, b, x, y, color, space);
}

void DebugDraw::FilledRect(float x, float y, float width, float height, rhi::Color color,
                           rhi::SpriteSpace space) {
    Push({Prim::Kind::FilledRect, x, y, width, height, color, space});
}

void DebugDraw::Circle(float centerX, float centerY, float radius, rhi::Color color,
                       int segments, rhi::SpriteSpace space) {
    if (segments < 3) segments = 3;
    constexpr float kTwoPi = 2.0f * std::numbers::pi_v<float>;
    float prevX = centerX + radius, prevY = centerY;
    for (int i = 1; i <= segments; ++i) {
        const float angle = kTwoPi * static_cast<float>(i) / static_cast<float>(segments);
        const float px = centerX + radius * std::cos(angle);
        const float py = centerY + radius * std::sin(angle);
        Line(prevX, prevY, px, py, color, space);
        prevX = px;
        prevY = py;
    }
}

void DebugDraw::Cross(float x, float y, float size, rhi::Color color,
                      rhi::SpriteSpace space) {
    Line(x - size, y, x + size, y, color, space);
    Line(x, y - size, x, y + size, color, space);
}

void DebugDraw::Arrow(float x0, float y0, float x1, float y1, rhi::Color color,
                      rhi::SpriteSpace space) {
    Line(x0, y0, x1, y1, color, space);

    const float dx = x1 - x0, dy = y1 - y0;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len <= 1e-4f) return; // 長さゼロは矢頭の向きが定まらない。線分（点）だけで終える

    // 矢頭: 終点から逆方向 ±30° に 2 本。長さは線分の 1/4（上限 12px）。
    const float headLen = len * 0.25f < 12.0f ? len * 0.25f : 12.0f;
    const float bx = -dx / len, by = -dy / len;
    constexpr float kCos = 0.86602540f; // cos(30°)
    constexpr float kSin = 0.5f;        // sin(30°)
    Line(x1, y1, x1 + headLen * (bx * kCos - by * kSin),
         y1 + headLen * (bx * kSin + by * kCos), color, space);
    Line(x1, y1, x1 + headLen * (bx * kCos + by * kSin),
         y1 + headLen * (-bx * kSin + by * kCos), color, space);
}

// ── フレーム制御 ─────────────────────────────────────────────────────────────

void DebugDraw::BeginFixedStep() {
    // ステップごとに積み直すことで、多重ステップフレーム（N > 1）でも
    // 最後のステップ分だけが描かれる（アルファの二重合成を防ぐ）。
    fixedPrims_.clear();
    inFixedStep_ = true;
}

void DebugDraw::EndFixedSteps() {
    inFixedStep_ = false;
}

void DebugDraw::Flush() {
    // テストパターンは毎フレーム側リストへ積む（Flush 末尾でクリアされる）。
    if (testPattern_) EmitTestPattern();
    // 固定側はクリアしない: ステップ 0 回のフレームでは前回分を描き続ける
    //（固定更新の内容がフレームレートに応じて点滅するのを防ぐ）。
    Submit(fixedPrims_);
    Submit(framePrims_);
    framePrims_.clear();
}

// ── 内部 ─────────────────────────────────────────────────────────────────────

void DebugDraw::Push(const Prim& prim) {
    (inFixedStep_ ? fixedPrims_ : framePrims_).push_back(prim);
}

void DebugDraw::Submit(const std::vector<Prim>& prims) {
    for (const Prim& p : prims) {
        switch (p.kind) {
        case Prim::Kind::Line:
            renderer_->SubmitLine({p.x0, p.y0, p.x1, p.y1, p.color, p.space});
            break;
        case Prim::Kind::FilledRect: {
            EnsureWhiteTexture();
            if (!whiteTexture_.IsValid()) break;
            rhi::SpriteDrawDesc desc;
            desc.texture = whiteTexture_;
            desc.x       = p.x0;
            desc.y       = p.y0;
            desc.width   = p.x1;
            desc.height  = p.y1;
            desc.color   = p.color;
            desc.space   = p.space;
            desc.sortKey = 0xFFFFFFFFu; // 同 space 内の最前面（線と ImGui はさらに手前）
            renderer_->SubmitSprite(desc);
            break;
        }
        }
    }
}

void DebugDraw::EnsureWhiteTexture() {
    if (whiteTexture_.IsValid() || whiteTextureFailed_) return;
    static constexpr uint8_t kWhitePixel[4] = {255, 255, 255, 255};
    if (auto tex = renderer_->CreateTexture(kWhitePixel, 1, 1)) {
        whiteTexture_ = *tex;
    } else {
        // 失敗を毎フレーム繰り返さない（テクスチャスロット枯渇等は他でも警告済み）。
        log::Warn("DebugDraw: failed to create the white texture: {}. "
                  "FilledRect will be skipped.", tex.error());
        whiteTextureFailed_ = true;
    }
}

void DebugDraw::EmitTestPattern() {
    // World 空間: グリッド + 各プリミティブ 1 種ずつ。カメラを動かすと追従し、
    // ズームしても線幅 1px のままであることを確認できる。
    const rhi::Color kGrid{0.5f, 0.5f, 0.5f, 0.5f};
    for (int i = 0; i <= 10; ++i) {
        const float t = static_cast<float>(i) * 32.0f;
        Line(t, 0.0f, t, 320.0f, kGrid);
        Line(0.0f, t, 320.0f, t, kGrid);
    }
    Rect(32.0f, 32.0f, 64.0f, 48.0f, {0.0f, 1.0f, 0.0f, 1.0f});
    FilledRect(128.0f, 32.0f, 64.0f, 48.0f, {1.0f, 0.0f, 0.0f, 0.5f});
    Circle(240.0f, 120.0f, 40.0f, {1.0f, 1.0f, 0.0f, 1.0f});
    Arrow(32.0f, 160.0f, 120.0f, 200.0f, {0.0f, 1.0f, 1.0f, 1.0f});
    Cross(200.0f, 200.0f, 8.0f, {1.0f, 0.0f, 1.0f, 1.0f});

    // Screen 空間: 仮想解像度の 4px 内側に枠。カメラに追従せず、
    // ウィンドウリサイズ時もレターボックス内側に張り付くことを確認できる。
    const float w = static_cast<float>(renderer_->VirtualWidth());
    const float h = static_cast<float>(renderer_->VirtualHeight());
    Rect(4.0f, 4.0f, w - 8.0f, h - 8.0f, {1.0f, 1.0f, 1.0f, 0.75f},
         rhi::SpriteSpace::Screen);
}

#else // !WITCH_DEBUG_DRAW — 全メソッド no-op（API は残し、呼び出し側の #ifdef を不要にする）

void DebugDraw::Line(float, float, float, float, rhi::Color, rhi::SpriteSpace) {}
void DebugDraw::Rect(float, float, float, float, rhi::Color, rhi::SpriteSpace) {}
void DebugDraw::FilledRect(float, float, float, float, rhi::Color, rhi::SpriteSpace) {}
void DebugDraw::Circle(float, float, float, rhi::Color, int, rhi::SpriteSpace) {}
void DebugDraw::Cross(float, float, float, rhi::Color, rhi::SpriteSpace) {}
void DebugDraw::Arrow(float, float, float, float, rhi::Color, rhi::SpriteSpace) {}
void DebugDraw::BeginFixedStep() {}
void DebugDraw::EndFixedSteps() {}
void DebugDraw::Flush() {}
void DebugDraw::Push(const Prim&) {}
void DebugDraw::Submit(const std::vector<Prim>&) {}
void DebugDraw::EnsureWhiteTexture() {}
void DebugDraw::EmitTestPattern() {}

#endif // WITCH_DEBUG_DRAW

} // namespace witch::debug
