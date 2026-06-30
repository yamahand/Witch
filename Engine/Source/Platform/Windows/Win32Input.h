#pragma once
#include "WitchEngine/Input/IInput.h"
#include <array>

namespace witch::platform {

/// IInput の Win32 実装。WndProc が（VK→Key 変換済みの）イベントを受け口へ流し込み、
/// Update() でフレーム境界のスナップショットを更新する。
///
/// 受け口群はメインスレッド（メッセージポンプ）からのみ呼ばれる前提で、追加の同期は持たない。
/// VK→Key 変換はプラットフォーム層（PlatformWindow.cpp）側が担い、本クラスは Key で受ける。
class Win32Input final : public IInput {
public:
    // ── 状態クエリ ─────────────────────────────────────────────────────────
    void Update() override;
    bool IsDown(Key key) const override;
    bool WasPressed(Key key) const override;
    bool WasReleased(Key key) const override;
    float MouseX() const override { return mouseX_; }
    float MouseY() const override { return mouseY_; }
    float MouseWheelDelta() const override { return wheelDelta_; }

    // ── イベント受け口（IInput, すべて抽象 Key / 数値で受ける）───────────────
    void OnKeyChange(Key key, bool down) override;
    void OnMouseMove(float x, float y) override;
    void OnMouseWheel(float delta) override;
    void ClearAll() override;

private:
    static void SetKey(std::array<bool, kKeyCount>& state, Key key, bool down);

    std::array<bool, kKeyCount> current_{};
    std::array<bool, kKeyCount> previous_{};
    float mouseX_     = 0.0f;
    float mouseY_     = 0.0f;
    float wheelDelta_ = 0.0f;
};

} // namespace witch::platform
