#pragma once
#include "WitchEngine/Graphics2D/Camera2D.h"

namespace witch {

/// カメラを管理するサービス。世界に 1 つ存在し、Services ロケーター越しに引く。
///
/// 現状は「アクティブカメラ 1 台」だけを保持する。以前は Scene が Camera2D を直接
/// メンバとして持っていたが、カメラを Scene から独立させることで、シーンをまたいだ
/// カメラ制御や（将来の）複数カメラ・分割画面への拡張余地を作る。
///
/// ビューポート同期は GameLoop がフレーム先頭で SetViewport を呼んで行う。
/// SpriteComponent はワールド→スクリーン変換のためにこのアクティブカメラを参照する。
///
/// 将来: 複数カメラの登録／アクティブ切替を足す（Issue: RenderManager 分離と併せて検討）。
class CameraManager {
public:
    /// 現在アクティブなカメラ。描画・入力ともにこれを操作／参照する。
    Camera2D& Active() { return active_; }
    const Camera2D& Active() const { return active_; }

    /// アクティブカメラのビューポート（描画先）サイズを設定する。
    /// GameLoop が現在の描画先サイズに毎フレーム同期する。
    void SetViewport(float width, float height) { active_.SetViewport(width, height); }

private:
    Camera2D active_;
};

} // namespace witch
