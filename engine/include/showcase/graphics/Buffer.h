#pragma once

#include <D3D12MemAlloc.h>

#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>

using Microsoft::WRL::ComPtr;

namespace showcase {

class Buffer {
public:
    [[nodiscard]] bool InitAsVertexBuffer(ID3D12Device* device, D3D12MA::Allocator* allocator,
                            const void* data, uint32_t size, uint32_t stride);
    [[nodiscard]] bool InitAsIndexBuffer(ID3D12Device* device, D3D12MA::Allocator* allocator,
                           const void* data, uint32_t size, DXGI_FORMAT format = DXGI_FORMAT_R32_UINT);
    [[nodiscard]] bool InitAsUploadBuffer(ID3D12Device* device, D3D12MA::Allocator* allocator, uint32_t size);
    void Shutdown();

    void UpdateData(const void* data, uint32_t size);
    void UpdateDataAtOffset(const void* data, uint32_t size, uint32_t offset);

    D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const { return m_vbView; }
    D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const { return m_ibView; }
    ID3D12Resource* GetResource() const { return m_resource.Get(); }

private:
    bool CreateUploadBuffer(ID3D12Device* device, D3D12MA::Allocator* allocator,
                            uint32_t size, const void* data);

    ComPtr<ID3D12Resource> m_resource;
    ComPtr<D3D12MA::Allocation> m_allocation;
    D3D12_VERTEX_BUFFER_VIEW m_vbView = {};
    D3D12_INDEX_BUFFER_VIEW m_ibView = {};
    void* m_mappedData = nullptr;
    uint32_t m_size = 0;
};

} // namespace showcase
