#pragma once
#define WIN32_LEAN_AND_MEAN
#include <d3d12.h>
#include "WitchEngine/Rhi/IRenderer.h"

namespace witch {

// Thin non-owning wrapper set up by D3D12Renderer each frame.
class D3D12CommandList : public rhi::ICommandList {
public:
    void Clear(const rhi::ClearDesc& desc) override;

    // Set by D3D12Renderer::BeginFrame before returning this object.
    ID3D12GraphicsCommandList* raw = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
};

} // namespace witch
