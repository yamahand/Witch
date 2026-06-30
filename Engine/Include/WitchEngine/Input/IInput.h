#pragma once
#include "WitchEngine/Input/Key.h"

namespace witch {

/// 入力サービスのインターフェース。Services 経由で取得する。
/// プラットフォーム差異は具象（Win32Input 等）に閉じ込め、上位コードはこの型だけを見る。
///
/// 状態は「現フレーム」と「前フレーム」の 2 スナップショットで管理し、
/// Engine がメインループ先頭（Time::Tick の直後・Scene::Update より前）で Update() を 1 回呼ぶ。
/// これにより同一フレーム内のすべての参照が一貫したエッジ判定を得る。
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
};

} // namespace witch
