#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <D3D12MemAlloc.h>
#include <showcase/graphics/DescriptorHeap.h>
#include <cstdint>

using Microsoft::WRL::ComPtr;

namespace showcase {

class Texture {
public:
    bool InitFromMemory(ID3D12Device* device, D3D12MA::Allocator* allocator,
                        ID3D12GraphicsCommandList* cmdList, DescriptorHeap& srvHeap,
                        const uint8_t* data, uint32_t width, uint32_t height,
                        uint32_t channels, DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);
    void ReleaseUploadResources();
    void Shutdown(DescriptorHeap& srvHeap);

    ID3D12Resource* GetResource() const { return m_resource.Get(); }
    DescriptorHandle GetSRVHandle() const { return m_srvHandle; }
    uint32_t GetWidth() const { return m_width; }
    uint32_t GetHeight() const { return m_height; }

private:
    ComPtr<ID3D12Resource> m_resource;
    ComPtr<D3D12MA::Allocation> m_allocation;
    ComPtr<ID3D12Resource> m_uploadResource;
    ComPtr<D3D12MA::Allocation> m_uploadAllocation;
    DescriptorHandle m_srvHandle;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

} // namespace showcase
