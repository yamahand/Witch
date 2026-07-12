#include "WitchEngine/Debug/DebugDraw.h"
#include "WitchEngine/Rhi/IRenderer.h"

namespace witch::debug {

DebugDraw::DebugDraw(rhi::IRenderer* renderer) : renderer_(renderer) {}

DebugDraw::~DebugDraw() = default;

#ifdef WITCH_DEBUG_DRAW

// ── 提出 API ─────────────────────────────────────────────────────────────────

void DebugDraw::Line(float x0, float y0, float x1, float y1, rhi::Color color,
                     rhi::SpriteSpace space) {
    Push({Prim::Kind::Line, x0, y0, x1, y1, color, space});
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
        case Prim::Kind::FilledRect:
            // M3（形状ヘルパー）で白テクスチャスプライトとして実装する。
            break;
        }
    }
}

#else // !WITCH_DEBUG_DRAW — 全メソッド no-op（API は残し、呼び出し側の #ifdef を不要にする）

void DebugDraw::Line(float, float, float, float, rhi::Color, rhi::SpriteSpace) {}
void DebugDraw::BeginFixedStep() {}
void DebugDraw::EndFixedSteps() {}
void DebugDraw::Flush() {}
void DebugDraw::Push(const Prim&) {}
void DebugDraw::Submit(const std::vector<Prim>&) {}

#endif // WITCH_DEBUG_DRAW

} // namespace witch::debug
