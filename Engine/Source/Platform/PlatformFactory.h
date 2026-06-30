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
/// Engine はこのインターフェースだけを見て、Win32Input / D3D12Renderer 等の
/// 具象ヘッダに直接依存しない（プラットフォーム差異は Platform/<OS>/ のファイル分割で吸収）。
/// 実装は OS ごとに用意し、CMake が対象ソースを選ぶ（現状 Windows のみ）。

/// このプラットフォームの入力サービスを生成する。Init 不要（生成即利用可）。
std::unique_ptr<IInput> CreatePlatformInput();

/// このプラットフォームのレンダラを生成する。生成のみ。
/// Init（ウィンドウハンドル・サイズ）の呼び出しは Engine 側が握る。
std::unique_ptr<rhi::IRenderer> CreatePlatformRenderer();

} // namespace witch::platform
