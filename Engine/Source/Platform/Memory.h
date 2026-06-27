#pragma once

namespace witch::platform {

// mimalloc によるグローバル new/delete 差し替えが実際に有効か検証する。
// 実装はプラットフォーム別ソースで提供する（現在は Windows のみ＝Memory.cpp）。
// CMake が OS ごとに対象ソースを選ぶため、呼び出し側に #ifdef は不要。
// この関数（mimalloc-new-delete.h を含む TU）への参照を作ることで、MSVC リンカが
// 静的ライブラリから Memory.obj を取りこぼす silent failure も防ぐ。
// Engine::Init() の先頭で 1 回呼ぶ。
void EnsureAllocatorActive();

}  // namespace witch::platform
