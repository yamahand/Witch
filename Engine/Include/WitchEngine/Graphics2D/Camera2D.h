#pragma once

namespace witch {

/// 2D カメラ。ワールド座標 → スクリーン座標（ピクセル, 左上原点）の変換を担う。
///
/// 設計方針: カメラ変換は CPU 側（SpriteComponent::Update）で適用し、RHI/HLSL は
/// スクリーン座標のまま保つ（RHI 隔離の鉄則）。RHI 層はカメラを一切知らない。
///
/// カメラ中心(x_, y_) がビューポート中央に来るように写像する。zoom > 1 で拡大。
class Camera2D {
public:
    /// カメラの注視点（ワールド座標）。
    void SetPosition(float x, float y) { x_ = x; y_ = y; }
    float X() const { return x_; }
    float Y() const { return y_; }

    /// 注視点を相対移動する。
    void Move(float dx, float dy) { x_ += dx; y_ += dy; }

    /// ズーム倍率（>0）。1.0 で等倍、2.0 で 2 倍に拡大表示。
    void SetZoom(float zoom) { zoom_ = zoom > 0.0f ? zoom : zoom_; }
    float Zoom() const { return zoom_; }

    /// ビューポート（描画先）サイズをピクセルで設定する。
    /// 通常はウィンドウサイズ。Engine/Scene がリサイズ時に更新する。
    void SetViewport(float width, float height) {
        viewportWidth_  = width;
        viewportHeight_ = height;
    }
    float ViewportWidth() const { return viewportWidth_; }
    float ViewportHeight() const { return viewportHeight_; }

    /// ワールド座標 → スクリーン座標（ピクセル, 左上原点）。
    /// カメラ中心がビューポート中央に写る。
    float WorldToScreenX(float worldX) const {
        return (worldX - x_) * zoom_ + viewportWidth_ * 0.5f;
    }
    float WorldToScreenY(float worldY) const {
        return (worldY - y_) * zoom_ + viewportHeight_ * 0.5f;
    }

    /// スクリーン座標 → ワールド座標（マウスピック等の逆変換）。
    float ScreenToWorldX(float screenX) const {
        return (screenX - viewportWidth_ * 0.5f) / zoom_ + x_;
    }
    float ScreenToWorldY(float screenY) const {
        return (screenY - viewportHeight_ * 0.5f) / zoom_ + y_;
    }

private:
    float x_ = 0.0f;
    float y_ = 0.0f;
    float zoom_ = 1.0f;
    float viewportWidth_  = 1280.0f;
    float viewportHeight_ = 720.0f;
};

} // namespace witch
