#include "Platform/PlatformFactory.h"
#include "Rhi/D3D12/D3D12Renderer.h"

// CreatePlatformRenderer() の実装はここ（Rhi/D3D12/ 内）に置く。
// D3D12 / dxgi の型・ヘッダを Rhi/D3D12/ の外へ漏らさない鉄則を守るため、
// D3D12Renderer.h の include はこの TU に閉じ込める（grep で D3D12 が外に出ない状態を維持）。

namespace witch::platform {

std::unique_ptr<rhi::IRenderer> CreatePlatformRenderer() {
    return std::make_unique<D3D12Renderer>();
}

} // namespace witch::platform
