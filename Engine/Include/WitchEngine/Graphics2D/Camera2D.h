#pragma once

namespace witch {

/// 2D カメラ。ワールド座標 → スクリーン座標（ピクセル, 左上原点）の変換を担う。
///
/// 設計方針: スプライトのカメラ変換は IRenderer::SetCamera 経由で頂点シェーダが
/// 適用する（GameLoop が毎フレーム ViewScale/ViewOffset を渡す）。
/// WorldToScreenX/Y・ScreenToWorldX/Y はマウスピック等の CPU 側単発変換用に残す。
/// RHI に渡すのは合成済みの scale + offset のみで、注視点やビューポートの
/// 概念は漏らさない（RHI 隔離の鉄則）。
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

    /// ズーム倍率。1.0 で等倍、2.0 で 2 倍に拡大表示。
    /// [kMinZoom, kMaxZoom] にクランプする。下限は反転・破綻防止、上限は連続入力で
    /// スプライトがビューポート外へ飛んで操作不能になるのを防ぐ。
    /// std::clamp/std::max は使わない: 公開ヘッダが <windows.h> の min/max マクロと衝突しうるため。
    void SetZoom(float zoom) {
        zoom_ = zoom < kMinZoom ? kMinZoom : (zoom > kMaxZoom ? kMaxZoom : zoom);
    }
    float Zoom() const { return zoom_; }

    static constexpr float kMinZoom = 0.01f;
    static constexpr float kMaxZoom = 100.0f;

    /// ビューポート（描画先）サイズをピクセルで設定する。
    /// 通常はウィンドウサイズ。Engine/Scene がリサイズ時に更新する。
    void SetViewport(float width, float height) {
        viewportWidth_  = width;
        viewportHeight_ = height;
    }
    float ViewportWidth() const { return viewportWidth_; }
    float ViewportHeight() const { return viewportHeight_; }

    /// ワールド座標 → スクリーン座標（ピクセル, 左上原点）。
    /// ズームアンカーは常に「ビューポート中心」固定（注視点 (x_,y_) がビューポート中央に写り、
    /// ズームはその中央を基準に拡縮する）。そのためカメラを動かしてからズームすると、
    /// 注視点から離れたスプライトは中央へ向かって流れて見える。
    /// マウスカーソル基準ズーム等が要る場合は、将来 SetZoom(zoom, anchorScreenX, anchorScreenY)
    /// のようにアンカーを渡せる形へ拡張する（GitHub Issue #6 で追跡）。
    float WorldToScreenX(float worldX) const {
        return worldX * ViewScale() + ViewOffsetX();
    }
    float WorldToScreenY(float worldY) const {
        return worldY * ViewScale() + ViewOffsetY();
    }

    /// ビュー変換の合成形 screen = world * ViewScale() + ViewOffset()。
    /// IRenderer::SetCamera へ渡す値。WorldToScreenX/Y と同一の変換
    /// （(w - pos) * zoom + viewport*0.5 の恒等変形）。
    float ViewScale()   const { return zoom_; }
    float ViewOffsetX() const { return viewportWidth_  * 0.5f - x_ * zoom_; }
    float ViewOffsetY() const { return viewportHeight_ * 0.5f - y_ * zoom_; }

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
