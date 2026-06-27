#pragma once

namespace witch {

// mimalloc によるグローバル new/delete 差し替えが実際に有効か検証する。
// Memory.cpp（mimalloc-new-delete.h を含む TU）への参照を作り、MSVC リンカが
// Memory.obj を静的ライブラリから取りこぼす silent failure を防ぐ役割も兼ねる。
// Engine::Init() の先頭で 1 回呼ぶ。
void EnsureAllocatorActive();

}  // namespace witch
