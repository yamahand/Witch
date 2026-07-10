#pragma once
#include "WitchEngine/Scene/Scene.h"
#include <cstdint>

namespace witch {

/// M6 タイルマップ / レベルロードのデモシーン。
/// OnEnter で Content/Stage/Sample1_1.ldtk をロードし、レベル全体が収まるよう
/// カメラを合わせる（背景クリア色もレベルの bgColor になる）。
/// WASD: カメラ / Q,E,ホイール: ズーム / G: シーン再入（再ロード確認）
/// Tab: EmptyScene（M5 デモ）へ遷移 / Escape: 終了
class StageScene : public Scene {
public:
    void OnEnter() override;
    /// 連続量の入力（IsDown + dt スケール）: カメラ移動・キーズーム。
    void FixedUpdate(float fixedDt) override;
    /// エッジ/瞬間量の入力（WasPressed / ホイール）。
    /// 固定側に置くと多重ステップフレームで二重発火する（Scene.h の契約参照）。
    void FrameUpdate(float dt) override;
    void OnExit() override;
};

} // namespace witch
