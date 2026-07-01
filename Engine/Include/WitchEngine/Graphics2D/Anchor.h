#pragma once

namespace witch {

/// スプライトの基準点（アンカー）。矩形内のどの点を transform 位置に合わせるか。
/// ズーム時はこの点を固定して拡縮する。
enum class Anchor {
    TopLeft,    TopCenter,    TopRight,
    CenterLeft, Center,       CenterRight,
    BottomLeft, BottomCenter, BottomRight,
};

/// アンカーの X 方向正規化係数（0=左, 0.5=中央, 1=右）。
constexpr float AnchorFactorX(Anchor a) {
    switch (a) {
    case Anchor::TopLeft:
    case Anchor::CenterLeft:
    case Anchor::BottomLeft:
        return 0.0f;
    case Anchor::TopCenter:
    case Anchor::Center:
    case Anchor::BottomCenter:
        return 0.5f;
    case Anchor::TopRight:
    case Anchor::CenterRight:
    case Anchor::BottomRight:
        return 1.0f;
    }
    return 0.5f; // 到達しない（全 enum を網羅済み）。
}

/// アンカーの Y 方向正規化係数（0=上, 0.5=中央, 1=下）。
constexpr float AnchorFactorY(Anchor a) {
    switch (a) {
    case Anchor::TopLeft:
    case Anchor::TopCenter:
    case Anchor::TopRight:
        return 0.0f;
    case Anchor::CenterLeft:
    case Anchor::Center:
    case Anchor::CenterRight:
        return 0.5f;
    case Anchor::BottomLeft:
    case Anchor::BottomCenter:
    case Anchor::BottomRight:
        return 1.0f;
    }
    return 0.5f; // 到達しない（全 enum を網羅済み）。
}

} // namespace witch
