#pragma once

namespace witch::platform {

// mimalloc によるグローバル new/delete 差し替えが実際に有効か検証する。
// 実装はプラットフォーム別ソースで提供する（現在は Windows のみ＝
// Engine/Source/Platform/Windows/Memory.cpp）。CMake が OS ごとに対象ソースを
// 選ぶため、呼び出し側に #ifdef は不要。
// この関数（mimalloc-new-delete.h を含む TU）への参照を作ることで、MSVC リンカが
// 静的ライブラリから Memory.obj を取りこぼす silent failure も防ぐ。
// Engine::Init() の先頭で 1 回呼ぶ。
//
// スコープは現在 Windows のみ（CLAUDE.md）。将来 Linux/macOS を足すときは、
// その OS の Platform ソースで本関数を実装する（mimalloc は LD_PRELOAD 等 OS 別の
// 方式になる）。未実装のままだとリンクエラーになるが、それは「実装を足せ」という
// 想定どおりの設計シグナルであり、no-op スタブを先回りで置かない（早すぎる一般化を避ける）。
void EnsureAllocatorActive();

}  // namespace witch::platform
