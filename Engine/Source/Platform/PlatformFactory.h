#pragma once
#include <memory>

namespace witch {
class IInput;
} // namespace witch

namespace witch::rhi {
class IRenderer;
} // namespace witch::rhi

namespace witch::platform {

/// プラットフォーム具象の生成を Platform 層に閉じ込めるファクトリ。
/// Engine はこのインターフェース（IInput / rhi::IRenderer）だけを見て、入力・RHI の
/// 具象ヘッダに直接依存しない（プラットフォーム差異は Platform/<OS>/ のファイル分割で吸収）。
/// 実装は具象ごとに分割し、CMake が対象ソースを選ぶ:
///   - 入力:   Platform/Windows/Win32InputFactory.cpp
///   - レンダラ: Rhi/<backend>/ 内の専用 TU（RHI 型をその外へ漏らさないため）

/// このプラットフォームの入力サービスを生成する。Init 不要（生成即利用可）。
std::unique_ptr<IInput> CreatePlatformInput();

/// このプラットフォームのレンダラを生成する。生成のみ。
/// Init（ウィンドウハンドル・サイズ）の呼び出しは Engine 側が握る。
std::unique_ptr<rhi::IRenderer> CreatePlatformRenderer();

} // namespace witch::platform
