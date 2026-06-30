#pragma once
#include "WitchEngine/Input/IInput.h"
#include <array>

namespace witch::platform {

/// IInput の Win32 実装。WndProc が捕まえた生メッセージを On* 受け口へ流し込み、
/// Update() でフレーム境界のスナップショットを更新する。
///
/// On* 群はメインスレッド（メッセージポンプ）からのみ呼ばれる前提で、追加の同期は持たない。
class Win32Input final : public IInput {
public:
    // ── IInput ───────────────────────────────────────────────────────────
    void Update() override;
    bool IsDown(Key key) const override;
    bool WasPressed(Key key) const override;
    bool WasReleased(Key key) const override;
    float MouseX() const override { return mouseX_; }
    float MouseY() const override { return mouseY_; }
    float MouseWheelDelta() const override { return wheelDelta_; }

    // ── WndProc からの受け口（生 Win32 値を渡す）─────────────────────────
    /// 仮想キーコード（VK_*）の押下／解放。未対応の VK は無視する。
    void OnKeyDown(unsigned int vk);
    void OnKeyUp(unsigned int vk);
    /// マウスボタンの押下／解放。
    void OnMouseButton(Key button, bool down);
    /// クライアント座標でのカーソル移動。
    void OnMouseMove(float x, float y);
    /// ホイール回転量を加算する（Update でリセットされるまで蓄積）。
    void OnMouseWheel(float delta);

private:
    static void SetKey(std::array<bool, kKeyCount>& state, Key key, bool down);

    std::array<bool, kKeyCount> current_{};
    std::array<bool, kKeyCount> previous_{};
    float mouseX_     = 0.0f;
    float mouseY_     = 0.0f;
    float wheelDelta_ = 0.0f;
};

} // namespace witch::platform
