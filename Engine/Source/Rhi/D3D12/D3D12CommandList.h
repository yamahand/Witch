#pragma once
#define WIN32_LEAN_AND_MEAN
#include <d3d12.h>
#include "WitchEngine/Rhi/IRenderer.h"

namespace witch {

class D3D12Renderer;

/// D3D12Renderer が毎フレーム設定する非所有の薄いラッパー。
/// 所有しないため寿命管理は D3D12Renderer が行う。
class D3D12CommandList : public rhi::ICommandList {
public:
    void Clear(const rhi::ClearDesc& desc) override;
    void FlushSprites() override;
#ifdef WITCH_DEBUG_DRAW
    void FlushLines() override;
#endif

    /// D3D12Renderer::BeginFrame が返す前に設定する。
    ID3D12GraphicsCommandList* raw      = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
    D3D12Renderer* renderer             = nullptr;
};

} // namespace witch
