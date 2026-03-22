#include <showcase/graphics/RenderTarget.h>
#include <showcase/core/Log.h>

namespace showcase {

// ── Init / Shutdown ──────────────────────────────────────────────────
bool RenderTarget::Init(ID3D12Device* device, D3D12MA::Allocator* allocator,
                         DescriptorHeap& srvHeap,
                         uint32_t width, uint32_t height, DXGI_FORMAT format) {
    m_format = format;

    if (!m_rtvHeap.Init(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 1, false)) {
        SE_LOG_ERROR("Failed to create RTV descriptor heap for render target");
        return false;
    }

    if (!CreateResources(device, allocator, srvHeap, width, height)) {
        return false;
    }

    SE_LOG_INFO("Render target initialized ({}x{})", width, height);
    return true;
}

void RenderTarget::Shutdown(DescriptorHeap& srvHeap) {
    if (m_srvHandle.IsValid()) {
        srvHeap.Free(m_srvHandle);
        m_srvHandle = {};
    }
    m_resource.Reset();
    m_allocation.Reset();
    m_rtvHeap.Shutdown();
}

// ── Resize ───────────────────────────────────────────────────────────
void RenderTarget::Resize(ID3D12Device* device, D3D12MA::Allocator* allocator,
                            DescriptorHeap& srvHeap,
                            uint32_t width, uint32_t height) {
    if (m_srvHandle.IsValid()) {
        srvHeap.Free(m_srvHandle);
        m_srvHandle = {};
    }
    m_resource.Reset();
    m_allocation.Reset();

    CreateResources(device, allocator, srvHeap, width, height);
    SE_LOG_INFO("Render target resized ({}x{})", width, height);
}

// ── Internal ─────────────────────────────────────────────────────────
bool RenderTarget::CreateResources(ID3D12Device* device, D3D12MA::Allocator* allocator,
                                    DescriptorHeap& srvHeap,
                                    uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;

    float clearColor[] = {0.05f, 0.05f, 0.08f, 1.0f};
    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = m_format;
    clearValue.Color[0] = clearColor[0];
    clearValue.Color[1] = clearColor[1];
    clearValue.Color[2] = clearColor[2];
    clearValue.Color[3] = clearColor[3];

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Width = width;
    resourceDesc.Height = height;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = m_format;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12MA::ALLOCATION_DESC allocDesc = {};
    allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

    HRESULT hr = allocator->CreateResource(
        &allocDesc, &resourceDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &clearValue,
        &m_allocation,
        IID_PPV_ARGS(&m_resource));

    if (FAILED(hr)) {
        SE_LOG_ERROR("Failed to create render target resource");
        return false;
    }

    // Create RTV
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = m_format;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    auto rtvHandle = m_rtvHeap.GetHandle(0);
    device->CreateRenderTargetView(m_resource.Get(), &rtvDesc, rtvHandle.cpu);

    // Create SRV in shared shader-visible heap
    m_srvHandle = srvHeap.Allocate();
    if (!m_srvHandle.IsValid()) {
        SE_LOG_ERROR("Failed to allocate SRV descriptor for render target");
        return false;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = m_format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;

    device->CreateShaderResourceView(m_resource.Get(), &srvDesc, m_srvHandle.cpu);

    return true;
}

} // namespace showcase
