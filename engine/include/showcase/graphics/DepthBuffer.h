#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <D3D12MemAlloc.h>
#include <showcase/graphics/DescriptorHeap.h>

using Microsoft::WRL::ComPtr;

namespace showcase {

class DepthBuffer {
public:
    bool Init(ID3D12Device* device, D3D12MA::Allocator* allocator,
              uint32_t width, uint32_t height,
              DXGI_FORMAT format = DXGI_FORMAT_D32_FLOAT);
    void Shutdown();
    void Resize(ID3D12Device* device, D3D12MA::Allocator* allocator,
                uint32_t width, uint32_t height);

    ID3D12Resource* GetResource() const { return m_resource.Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const { return m_dsvHeap.GetHandle(0).cpu; }
    DXGI_FORMAT GetFormat() const { return m_format; }

private:
    bool CreateResources(ID3D12Device* device, D3D12MA::Allocator* allocator,
                         uint32_t width, uint32_t height);

    ComPtr<ID3D12Resource> m_resource;
    ComPtr<D3D12MA::Allocation> m_allocation;
    DescriptorHeap m_dsvHeap;
    DXGI_FORMAT m_format = DXGI_FORMAT_D32_FLOAT;
};

} // namespace showcase
