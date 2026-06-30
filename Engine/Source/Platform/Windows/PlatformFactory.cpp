#include "Platform/PlatformFactory.h"
#include "Platform/Windows/Win32Input.h"
#include "Rhi/D3D12/D3D12Renderer.h"

// このファイルは Windows 専用 TU（CMake の if(WIN32) でのみビルド）。
// Win32Input / D3D12Renderer といった具象ヘッダへの依存をここに閉じ込め、
// Engine.cpp などの上位コードがプラットフォーム具象を直接 include しないようにする。

namespace witch::platform {

std::unique_ptr<IInput> CreatePlatformInput() {
    return std::make_unique<Win32Input>();
}

std::unique_ptr<rhi::IRenderer> CreatePlatformRenderer() {
    return std::make_unique<D3D12Renderer>();
}

} // namespace witch::platform
