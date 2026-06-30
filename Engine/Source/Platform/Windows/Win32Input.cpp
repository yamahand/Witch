#include "Platform/Windows/Win32Input.h"

namespace witch::platform {

void Win32Input::SetKey(std::array<bool, kKeyCount>& state, Key key, bool down) {
    if (key == Key::Count)
        return;
    state[static_cast<std::size_t>(key)] = down;
}

void Win32Input::Update() {
    // PumpMessages の「前」に呼ばれる前提（Engine::Run 参照）。ここで前フレームの
    // current_ を previous_ に確定し、直後の PumpMessages が今フレームの入力を current_ に積む。
    previous_   = current_;
    wheelDelta_ = 0.0f; // ホイールは 1 フレーム限りの量。世代更新でリセット。
}

bool Win32Input::IsDown(Key key) const {
    if (key == Key::Count)
        return false;
    return current_[static_cast<std::size_t>(key)];
}

bool Win32Input::WasPressed(Key key) const {
    if (key == Key::Count)
        return false;
    const auto i = static_cast<std::size_t>(key);
    return current_[i] && !previous_[i];
}

bool Win32Input::WasReleased(Key key) const {
    if (key == Key::Count)
        return false;
    const auto i = static_cast<std::size_t>(key);
    return !current_[i] && previous_[i];
}

void Win32Input::OnKeyChange(Key key, bool down) {
    SetKey(current_, key, down);
}

void Win32Input::OnMouseMove(float x, float y) {
    mouseX_ = x;
    mouseY_ = y;
}

void Win32Input::OnMouseWheel(float delta) {
    wheelDelta_ += delta;
}

void Win32Input::ClearAll() {
    // フォーカス喪失時に呼ばれる。previous_ は触らない（次フレームの Update で
    // current_ の全 false が previous_ に伝播し、WasReleased が一度だけ立つ）。
    current_.fill(false);
    wheelDelta_ = 0.0f;
}

} // namespace witch::platform
