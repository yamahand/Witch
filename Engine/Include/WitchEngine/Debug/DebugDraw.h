#pragma once
#include "WitchEngine/Rhi/RhiTypes.h"
#include <vector>

namespace witch::rhi {
class IRenderer;
} // namespace witch::rhi

namespace witch::debug {

/// immediate-mode のデバッグプリミティブ描画サービス。
/// 表示したいフレームごとに毎回呼び直す（呼ばなくなれば消える）。
/// 全スプライトの手前・ImGui の奥に描かれる。Services::Instance().debugDraw で引く。
///
/// WITCH_DEBUG_DRAW が OFF のビルドでは全メソッドが no-op になる
/// （API 自体は常に存在するため、呼び出し側に #ifdef は不要）。
///
/// 固定ステップ（FixedUpdate、フレーム内 0〜N 回）から呼んでも安全:
/// 固定側の提出は最後のステップ分だけが残り（二重描画しない）、
/// ステップ 0 回のフレームでは前回分が持続する（点滅しない）。
class DebugDraw {
public:
    explicit DebugDraw(rhi::IRenderer* renderer);
    ~DebugDraw(); ///< 遅延生成した白テクスチャを解放する（renderer より先に破棄すること）

    DebugDraw(const DebugDraw&) = delete;
    DebugDraw& operator=(const DebugDraw&) = delete;

    // ── 提出 API（ゲーム / コンポーネント / DrawDebugUI から毎フレーム呼ぶ）────
    /// 線分。線幅は常に 1px（ズームしても太らない）。
    void Line(float x0, float y0, float x1, float y1, rhi::Color color,
              rhi::SpriteSpace space = rhi::SpriteSpace::World);
    /// 矩形の輪郭（左上 + サイズ）。当たり判定 AABB の可視化などに。
    void Rect(float x, float y, float width, float height, rhi::Color color,
              rhi::SpriteSpace space = rhi::SpriteSpace::World);
    /// 塗りつぶし矩形（左上 + サイズ）。半透明色で範囲のハイライトなどに。
    /// 線と違いスプライトとして描かれるため、同 space の全スプライトの最前面
    ///（ただし線プリミティブと Screen 空間 HUD よりは奥）になる。
    void FilledRect(float x, float y, float width, float height, rhi::Color color,
                    rhi::SpriteSpace space = rhi::SpriteSpace::World);
    /// 円の輪郭（線分近似）。segments は 3 以上にクランプされる。
    void Circle(float centerX, float centerY, float radius, rhi::Color color,
                int segments = 24, rhi::SpriteSpace space = rhi::SpriteSpace::World);
    /// ＋印の位置マーカー。size は中心から先端までの長さ。
    void Cross(float x, float y, float size, rhi::Color color,
               rhi::SpriteSpace space = rhi::SpriteSpace::World);
    /// 矢印（線分 + 矢頭 2 本）。速度・向きの可視化などに。
    void Arrow(float x0, float y0, float x1, float y1, rhi::Color color,
               rhi::SpriteSpace space = rhi::SpriteSpace::World);

    /// 動作確認用テストパターンの表示切替（DebugMenu の "DebugDraw Test"）。
    void SetTestPattern(bool enabled) { testPattern_ = enabled; }
    bool TestPatternEnabled() const { return testPattern_; }

    // ── フレーム制御（GameLoop 専用。ゲームコードは呼ばない）─────────────────
    /// 固定ステップ開始: 固定リストをクリアし、以降の提出を固定側へ積む。
    void BeginFixedStep();
    /// 全固定ステップ終了: 以降の提出を毎フレーム側へ積む。
    void EndFixedSteps();
    /// 蓄積した全プリミティブを RHI へ提出し、毎フレーム側リストをクリアする。
    void Flush();

private:
    /// 蓄積するプリミティブ 1 個。Circle 等の形状は提出時に線分へ展開されるため、
    /// 保持するのは線分と塗り矩形の 2 種のみ。
    struct Prim {
        enum class Kind : uint8_t { Line, FilledRect } kind = Kind::Line;
        float x0 = 0, y0 = 0; ///< Line: 始点 / FilledRect: 左上
        float x1 = 0, y1 = 0; ///< Line: 終点 / FilledRect: 幅・高さ
        rhi::Color color = {1.0f, 1.0f, 1.0f, 1.0f};
        rhi::SpriteSpace space = rhi::SpriteSpace::World;
    };

    void Push(const Prim& prim);
    void Submit(const std::vector<Prim>& prims);
    /// FilledRect 用 1x1 白テクスチャを遅延生成する。失敗したら以降は再試行しない。
    void EnsureWhiteTexture();
    void EmitTestPattern();

    rhi::IRenderer* renderer_ = nullptr;
    /// 固定ステップ（FixedUpdate）からの提出分。BeginFixedStep ごとにクリア。
    std::vector<Prim> fixedPrims_;
    /// 毎フレーム側（FrameUpdate / Render フェーズ / DrawDebugUI）からの提出分。Flush でクリア。
    std::vector<Prim> framePrims_;
    bool inFixedStep_ = false;
    bool testPattern_ = false;
    rhi::TextureHandle whiteTexture_;
    bool whiteTextureFailed_ = false;
};

} // namespace witch::debug
