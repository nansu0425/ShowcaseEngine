#include <showcase/graphics/CubemapDepthBuffer.h>

#include <showcase/core/Log.h>

namespace showcase {

bool CubemapDepthBuffer::Init(ID3D12Device* device, D3D12MA::Allocator* allocator, uint32_t resolution,
                              DXGI_FORMAT format) {
    m_format = format;

    if (!m_dsvHeap.Init({device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, kNumFaces, false})) {
        SE_LOG_ERROR("Failed to create cubemap DSV descriptor heap");
        return false;
    }

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = m_format;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Width = resolution;
    resourceDesc.Height = resolution;
    resourceDesc.DepthOrArraySize = kNumFaces;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = m_format;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12MA::ALLOCATION_DESC allocDesc = {};
    allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

    HRESULT hr = allocator->CreateResource(&allocDesc, &resourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue,
                                           &m_allocation, IID_PPV_ARGS(&m_resource));
    if (FAILED(hr)) {
        SE_LOG_ERROR("Failed to create cubemap depth buffer resource");
        return false;
    }

    // Create one DSV per face
    for (uint32_t face = 0; face < kNumFaces; ++face) {
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = m_format;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
        dsvDesc.Texture2DArray.MipSlice = 0;
        dsvDesc.Texture2DArray.FirstArraySlice = face;
        dsvDesc.Texture2DArray.ArraySize = 1;

        auto handle = m_dsvHeap.GetHandle(face);
        device->CreateDepthStencilView(m_resource.Get(), &dsvDesc, handle.cpu);
    }

    SE_LOG_INFO("Cubemap depth buffer initialized ({}x{}x6)", resolution, resolution);
    return true;
}

void CubemapDepthBuffer::Shutdown(DescriptorHeap& srvHeap) {
    if (m_srvHandle.IsValid()) {
        srvHeap.Free(m_srvHandle);
        m_srvHandle = {};
    }
    m_resource.Reset();
    m_allocation.Reset();
    m_dsvHeap.Shutdown();
}

void CubemapDepthBuffer::CreateSRV(ID3D12Device* device, DescriptorHeap& srvHeap) {
    if (!m_srvHandle.IsValid()) {
        m_srvHandle = srvHeap.Allocate();
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.MipLevels = 1;

    device->CreateShaderResourceView(m_resource.Get(), &srvDesc, m_srvHandle.cpu);
}

} // namespace showcase
