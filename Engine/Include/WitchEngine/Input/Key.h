#pragma once
#include <cstddef>

namespace witch {

/// プラットフォーム非依存の入力キー。
/// Win32 の仮想キーコード（VK_*）には依存しない抽象。具象側（Win32Input）が
/// VK → Key の対応表を内部に持つ。マウスボタンも簡潔さを優先して Key に含める。
///
/// 末尾の Count は要素数を取るためのセンチネル。enum の途中に値を割り込ませない
/// （連番であることに kKeyCount / 状態配列のサイズが依存する）。
enum class Key {
    // ── Letters ──────────────────────────────────────────────────────────
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    // ── Digits（メインキーボード列）─────────────────────────────────────
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

    // ── Arrows ───────────────────────────────────────────────────────────
    Left, Right, Up, Down,

    // ── Common control keys ──────────────────────────────────────────────
    Space, Enter, Escape, Tab, Backspace,
    LeftShift, LeftControl, LeftAlt,

    // ── Mouse buttons ────────────────────────────────────────────────────
    MouseLeft, MouseRight, MouseMiddle,

    Count // ← センチネル。実キーではない。常に最後。
};

/// Key の総数。状態配列（current/previous）のサイズに使う。
inline constexpr std::size_t kKeyCount = static_cast<std::size_t>(Key::Count);

// Win32Input の VK→Key 変換は A..Z / Num0..9 が連番であることに依存する。
// 途中にキーが挿入されてもコンパイル時に気づけるよう、契約を static_assert で固定する。
static_assert(static_cast<int>(Key::B) == static_cast<int>(Key::A) + 1,
              "Key enum must be contiguous from A (Win32Input relies on this).");
static_assert(static_cast<int>(Key::Z) == static_cast<int>(Key::A) + 25,
              "Key enum A..Z must be contiguous (Win32Input relies on this).");
static_assert(static_cast<int>(Key::Num1) == static_cast<int>(Key::Num0) + 1,
              "Key enum must be contiguous from Num0 (Win32Input relies on this).");
static_assert(static_cast<int>(Key::Num9) == static_cast<int>(Key::Num0) + 9,
              "Key enum Num0..Num9 must be contiguous (Win32Input relies on this).");

} // namespace witch
