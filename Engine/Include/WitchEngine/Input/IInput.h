#pragma once
#include "WitchEngine/Input/Key.h"

namespace witch {

/// 入力サービスのインターフェース。Services 経由で取得する。
/// プラットフォーム差異は具象（Win32Input 等）に閉じ込め、上位コードはこの型だけを見る。
///
/// 状態は「現フレーム」と「前フレーム」の 2 スナップショットで管理し、
/// Engine がメインループ先頭で、OS メッセージのポンプ（PumpMessages）より「前」に
/// Update() を 1 回呼ぶ。Update() で current → previous を確定してから今フレームの
/// 入力を current に取り込むことで、Scene::Update でのエッジ判定（current vs previous）が
/// 正しく出る。順序を逆にすると差分が即消えて WasPressed/WasReleased が常に false になる。
class IInput {
public:
    virtual ~IInput() = default;

    /// このフレームの入力状態を確定する。Engine がループ先頭で 1 回だけ呼ぶ。
    /// current → previous をコピーし、ホイール量など 1 フレーム限りの値をリセットする。
    virtual void Update() = 0;

    /// キーが押されている（継続）。
    virtual bool IsDown(Key key) const = 0;
    /// このフレームで押された瞬間（前フレーム up → 今フレーム down）。
    virtual bool WasPressed(Key key) const = 0;
    /// このフレームで離された瞬間（前フレーム down → 今フレーム up）。
    virtual bool WasReleased(Key key) const = 0;

    /// マウスカーソルのクライアント座標（ピクセル）。
    virtual float MouseX() const = 0;
    virtual float MouseY() const = 0;
    /// このフレームのホイール回転量（前方向が正、WHEEL_DELTA=120 単位で正規化）。
    virtual float MouseWheelDelta() const = 0;

    // ── イベント受け口 ───────────────────────────────────────────────────
    // プラットフォーム層（WndProc 等）が生の OS イベントを「抽象キー Key」に
    // 変換してから呼ぶ。VK などプラットフォーム固有コードはここに持ち込まない
    // （IInput はプラットフォーム非依存を保つ）。これにより上位コードは IInput*
    // 越しに受け口を呼べ、具象へのダウンキャストが不要になる。

    /// キー／ボタンの押下状態が変化した（down=true で押下、false で解放）。
    virtual void OnKeyChange(Key key, bool down) = 0;
    /// マウスカーソルが移動した（クライアント座標, ピクセル）。
    virtual void OnMouseMove(float x, float y) = 0;
    /// ホイールが回転した（WHEEL_DELTA 単位で正規化済み、前方向が正）。
    virtual void OnMouseWheel(float delta) = 0;
    /// すべての押下状態をクリアする（フォーカス喪失時など）。
    virtual void ClearAll() = 0;
};

} // namespace witch
