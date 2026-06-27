#pragma once

namespace witch {

// mimalloc によるグローバル new/delete 差し替えが実際に有効か検証する。
// Memory.cpp（mimalloc-new-delete.h を含む TU）への参照を作り、MSVC リンカが
// Memory.obj を静的ライブラリから取りこぼす silent failure を防ぐ役割も兼ねる。
// Engine::Init() の先頭で 1 回呼ぶ。
//
// 実装は Windows 専用（mimalloc 導入は Win32 のみ。CMakeLists の if(WIN32) で
// Memory.cpp をビルド対象にしている）。他 OS では実体が無くリンクできないため、
// 非 Windows では no-op の inline 定義にして呼び出し側を無条件に保つ。
#ifdef _WIN32
void EnsureAllocatorActive();
#else
inline void EnsureAllocatorActive() {}
#endif

}  // namespace witch
