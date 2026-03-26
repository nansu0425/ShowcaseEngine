#pragma once

#include <showcase/graphics/DescriptorHeap.h>

#include <D3D12MemAlloc.h>

#include <d3d12.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace showcase {

class RenderTarget {
public:
    [[nodiscard]] bool Init(ID3D12Device* device, D3D12MA::Allocator* allocator, DescriptorHeap& srvHeap,
                            uint32_t width, uint32_t height, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM,
                            const float* clearColor = nullptr);
    void Shutdown(DescriptorHeap& srvHeap);
    void Resize(ID3D12Device* device, D3D12MA::Allocator* allocator, DescriptorHeap& srvHeap, uint32_t width,
                uint32_t height);

    ID3D12Resource* GetResource() const { return m_resource.Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTV() const { return m_rtvHeap.GetHandle(0).cpu; }
    DescriptorHandle GetSRVHandle() const { return m_srvHandle; }
    DXGI_FORMAT GetFormat() const { return m_format; }
    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }

private:
    bool CreateResources(ID3D12Device* device, D3D12MA::Allocator* allocator, DescriptorHeap& srvHeap, uint32_t width,
                         uint32_t height);

    ComPtr<ID3D12Resource> m_resource;
    ComPtr<D3D12MA::Allocation> m_allocation;
    DescriptorHeap m_rtvHeap;
    DescriptorHandle m_srvHandle;
    DXGI_FORMAT m_format = DXGI_FORMAT_R8G8B8A8_UNORM;
    float m_clearColor[4] = {0.05f, 0.05f, 0.08f, 1.0f};
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

} // namespace showcase
