#pragma once

#include <showcase/graphics/DescriptorHeap.h>

#include <D3D12MemAlloc.h>

#include <d3d12.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace showcase {

struct DepthBufferDesc {
    ID3D12Device* device = nullptr;
    D3D12MA::Allocator* allocator = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    DXGI_FORMAT format = DXGI_FORMAT_D32_FLOAT;
};

class DepthBuffer {
public:
    [[nodiscard]] bool Init(const DepthBufferDesc& desc);
    void Shutdown(DescriptorHeap& srvHeap);
    void Resize(ID3D12Device* device, D3D12MA::Allocator* allocator, uint32_t width, uint32_t height);

    void CreateSRV(ID3D12Device* device, DescriptorHeap& srvHeap);

    ID3D12Resource* GetResource() const { return m_resource.Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const { return m_dsvHeap.GetHandle(0).cpu; }
    DXGI_FORMAT GetFormat() const { return m_format; }
    DescriptorHandle GetSRV() const { return m_srvHandle; }

private:
    bool CreateResources(ID3D12Device* device, D3D12MA::Allocator* allocator, uint32_t width, uint32_t height);

    ComPtr<ID3D12Resource> m_resource;
    ComPtr<D3D12MA::Allocation> m_allocation;
    DescriptorHeap m_dsvHeap;
    DescriptorHandle m_srvHandle;
    DXGI_FORMAT m_format = DXGI_FORMAT_D32_FLOAT;
};

} // namespace showcase
