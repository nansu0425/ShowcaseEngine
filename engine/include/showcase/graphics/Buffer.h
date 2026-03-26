#pragma once

#include <D3D12MemAlloc.h>

#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>

using Microsoft::WRL::ComPtr;

namespace showcase {

struct VertexBufferDesc {
    ID3D12Device* device = nullptr;
    D3D12MA::Allocator* allocator = nullptr;
    const void* data = nullptr;
    uint32_t size = 0;
    uint32_t stride = 0;
};

struct IndexBufferDesc {
    ID3D12Device* device = nullptr;
    D3D12MA::Allocator* allocator = nullptr;
    const void* data = nullptr;
    uint32_t size = 0;
    DXGI_FORMAT format = DXGI_FORMAT_R32_UINT;
};

class Buffer {
public:
    [[nodiscard]] bool InitAsVertexBuffer(const VertexBufferDesc& desc);
    [[nodiscard]] bool InitAsIndexBuffer(const IndexBufferDesc& desc);
    [[nodiscard]] bool InitAsUploadBuffer(ID3D12Device* device, D3D12MA::Allocator* allocator, uint32_t size);
    void Shutdown();

    /// Copies data to the mapped upload buffer (CPU-visible, no GPU sync required).
    void UpdateData(const void* data, uint32_t size);
    /// Copies data at a 256-byte-aligned offset within the mapped upload buffer.
    void UpdateDataAtOffset(const void* data, uint32_t size, uint32_t offset);

    D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const { return m_vbView; }
    D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const { return m_ibView; }
    ID3D12Resource* GetResource() const { return m_resource.Get(); }

private:
    bool CreateUploadBuffer(ID3D12Device* device, D3D12MA::Allocator* allocator, uint32_t size, const void* data);

    ComPtr<ID3D12Resource> m_resource;
    ComPtr<D3D12MA::Allocation> m_allocation;
    D3D12_VERTEX_BUFFER_VIEW m_vbView = {};
    D3D12_INDEX_BUFFER_VIEW m_ibView = {};
    void* m_mappedData = nullptr;
    uint32_t m_size = 0;
};

} // namespace showcase
