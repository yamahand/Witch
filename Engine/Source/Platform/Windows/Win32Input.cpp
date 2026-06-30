#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "Platform/Windows/Win32Input.h"

namespace witch::platform {

namespace {

/// VK_* → Key。未対応コードは Key::Count を返し、呼び出し側が無視する。
Key VkToKey(unsigned int vk) {
    // A–Z / 0–9 は ASCII と一致する（'A'..'Z', '0'..'9'）。
    if (vk >= 'A' && vk <= 'Z')
        return static_cast<Key>(static_cast<int>(Key::A) + (vk - 'A'));
    if (vk >= '0' && vk <= '9')
        return static_cast<Key>(static_cast<int>(Key::Num0) + (vk - '0'));

    switch (vk) {
    case VK_LEFT:     return Key::Left;
    case VK_RIGHT:    return Key::Right;
    case VK_UP:       return Key::Up;
    case VK_DOWN:     return Key::Down;
    case VK_SPACE:    return Key::Space;
    case VK_RETURN:   return Key::Enter;
    case VK_ESCAPE:   return Key::Escape;
    case VK_TAB:      return Key::Tab;
    case VK_BACK:     return Key::Backspace;
    case VK_SHIFT:    return Key::LeftShift;
    case VK_LSHIFT:   return Key::LeftShift;
    case VK_CONTROL:  return Key::LeftControl;
    case VK_LCONTROL: return Key::LeftControl;
    case VK_MENU:     return Key::LeftAlt;
    case VK_LMENU:    return Key::LeftAlt;
    // VK_RSHIFT / VK_RCONTROL / VK_RMENU は Key enum に右側キーが未定義のため
    // ここで無視される（Key::Count に落ちて SetKey で破棄）。
    // 将来 RightShift 等を Key に追加したら、この switch にも対応 case を足すこと。
    default:          return Key::Count;
    }
}

} // namespace

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

void Win32Input::OnKeyDown(unsigned int vk) {
    SetKey(current_, VkToKey(vk), true);
}

void Win32Input::OnKeyUp(unsigned int vk) {
    SetKey(current_, VkToKey(vk), false);
}

void Win32Input::OnMouseButton(Key button, bool down) {
    SetKey(current_, button, down);
}

void Win32Input::OnMouseMove(float x, float y) {
    mouseX_ = x;
    mouseY_ = y;
}

void Win32Input::OnMouseWheel(float delta) {
    wheelDelta_ += delta;
}

} // namespace witch::platform
