#pragma once

#include <showcase/graphics/DescriptorHeap.h>

#include <D3D12MemAlloc.h>

#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>

using Microsoft::WRL::ComPtr;

namespace showcase {

class CubemapDepthBuffer {
public:
    static constexpr uint32_t kNumFaces = 6;

    [[nodiscard]] bool Init(ID3D12Device* device, D3D12MA::Allocator* allocator, uint32_t resolution,
                            DXGI_FORMAT format = DXGI_FORMAT_D32_FLOAT);
    void Shutdown(DescriptorHeap& srvHeap);

    void CreateSRV(ID3D12Device* device, DescriptorHeap& srvHeap);

    ID3D12Resource* GetResource() const { return m_resource.Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetFaceDSV(uint32_t face) const { return m_dsvHeap.GetHandle(face).cpu; }
    DescriptorHandle GetSRV() const { return m_srvHandle; }

private:
    ComPtr<ID3D12Resource> m_resource;
    ComPtr<D3D12MA::Allocation> m_allocation;
    DescriptorHeap m_dsvHeap;     // 6 DSV descriptors (one per face)
    DescriptorHandle m_srvHandle; // single TextureCube SRV
    DXGI_FORMAT m_format = DXGI_FORMAT_D32_FLOAT;
};

} // namespace showcase
