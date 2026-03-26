#include <showcase/graphics/DepthBuffer.h>

#include <showcase/core/Log.h>

namespace showcase {

// ── Init / Shutdown ──────────────────────────────────────────────────
bool DepthBuffer::Init(const DepthBufferDesc& desc) {
    m_format = desc.format;

    if (!m_dsvHeap.Init({desc.device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false})) {
        SE_LOG_ERROR("Failed to create DSV descriptor heap");
        return false;
    }

    if (!CreateResources(desc.device, desc.allocator, desc.width, desc.height)) {
        return false;
    }

    SE_LOG_INFO("Depth buffer initialized ({}x{})", desc.width, desc.height);
    return true;
}

void DepthBuffer::Shutdown(DescriptorHeap& srvHeap) {
    if (m_srvHandle.IsValid()) {
        srvHeap.Free(m_srvHandle);
        m_srvHandle = {};
    }
    m_resource.Reset();
    m_allocation.Reset();
    m_dsvHeap.Shutdown();
}

// ── SRV ─────────────────────────────────────────────────────────────
void DepthBuffer::CreateSRV(ID3D12Device* device, DescriptorHeap& srvHeap) {
    if (!m_srvHandle.IsValid()) {
        m_srvHandle = srvHeap.Allocate();
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;

    device->CreateShaderResourceView(m_resource.Get(), &srvDesc, m_srvHandle.cpu);
}

// ── Resize ───────────────────────────────────────────────────────────
void DepthBuffer::Resize(ID3D12Device* device, D3D12MA::Allocator* allocator, uint32_t width, uint32_t height) {
    m_resource.Reset();
    m_allocation.Reset();
    CreateResources(device, allocator, width, height);
    SE_LOG_INFO("Depth buffer resized ({}x{})", width, height);
}

// ── Internal ─────────────────────────────────────────────────────────
bool DepthBuffer::CreateResources(ID3D12Device* device, D3D12MA::Allocator* allocator, uint32_t width,
                                  uint32_t height) {
    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = m_format;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Width = width;
    resourceDesc.Height = height;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = m_format;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12MA::ALLOCATION_DESC allocDesc = {};
    allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

    HRESULT hr = allocator->CreateResource(&allocDesc, &resourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue,
                                           &m_allocation, IID_PPV_ARGS(&m_resource));

    if (FAILED(hr)) {
        SE_LOG_ERROR("Failed to create depth buffer resource");
        return false;
    }

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = m_format;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

    auto handle = m_dsvHeap.GetHandle(0);
    device->CreateDepthStencilView(m_resource.Get(), &dsvDesc, handle.cpu);

    return true;
}

} // namespace showcase
