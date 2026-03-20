#include <showcase/graphics/Buffer.h>
#include <showcase/core/Log.h>
#include <cstring>

namespace showcase {

bool Buffer::InitAsVertexBuffer(ID3D12Device* device, D3D12MA::Allocator* allocator,
                                 const void* data, uint32_t size, uint32_t stride) {
    if (!CreateUploadBuffer(device, allocator, size, data)) {
        return false;
    }

    m_vbView.BufferLocation = m_resource->GetGPUVirtualAddress();
    m_vbView.SizeInBytes = size;
    m_vbView.StrideInBytes = stride;
    return true;
}

bool Buffer::InitAsIndexBuffer(ID3D12Device* device, D3D12MA::Allocator* allocator,
                                const void* data, uint32_t size, DXGI_FORMAT format) {
    if (!CreateUploadBuffer(device, allocator, size, data)) {
        return false;
    }

    m_ibView.BufferLocation = m_resource->GetGPUVirtualAddress();
    m_ibView.SizeInBytes = size;
    m_ibView.Format = format;
    return true;
}

bool Buffer::InitAsUploadBuffer(ID3D12Device* device, D3D12MA::Allocator* allocator, uint32_t size) {
    return CreateUploadBuffer(device, allocator, size, nullptr);
}

void Buffer::Shutdown() {
    if (m_mappedData && m_resource) {
        m_resource->Unmap(0, nullptr);
        m_mappedData = nullptr;
    }
    m_allocation.Reset();
    m_resource.Reset();
}

void Buffer::UpdateData(const void* data, uint32_t size) {
    if (m_mappedData && data) {
        memcpy(m_mappedData, data, size);
    }
}

bool Buffer::CreateUploadBuffer(ID3D12Device* device, D3D12MA::Allocator* allocator,
                                 uint32_t size, const void* data) {
    m_size = size;

    D3D12MA::ALLOCATION_DESC allocDesc = {};
    allocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = size;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc = {1, 0};
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = allocator->CreateResource(
        &allocDesc, &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        &m_allocation,
        IID_PPV_ARGS(&m_resource));

    if (FAILED(hr)) {
        SE_LOG_ERROR("Failed to create upload buffer");
        return false;
    }

    // Map the buffer
    D3D12_RANGE readRange = {0, 0};
    m_resource->Map(0, &readRange, &m_mappedData);

    if (data) {
        memcpy(m_mappedData, data, size);
    }

    return true;
}

} // namespace showcase
