#include <showcase/graphics/Texture.h>
#include <showcase/core/Log.h>

namespace showcase {

bool Texture::InitFromMemory(ID3D12Device* device, D3D12MA::Allocator* allocator,
                              ID3D12GraphicsCommandList* cmdList, DescriptorHeap& srvHeap,
                              const uint8_t* data, uint32_t width, uint32_t height,
                              uint32_t channels, DXGI_FORMAT format) {
    m_width = width;
    m_height = height;

    // Expand RGB to RGBA if needed
    std::vector<uint8_t> rgbaData;
    const uint8_t* srcData = data;
    if (channels == 3) {
        rgbaData.resize(width * height * 4);
        for (uint32_t i = 0; i < width * height; ++i) {
            rgbaData[i * 4 + 0] = data[i * 3 + 0];
            rgbaData[i * 4 + 1] = data[i * 3 + 1];
            rgbaData[i * 4 + 2] = data[i * 3 + 2];
            rgbaData[i * 4 + 3] = 255;
        }
        srcData = rgbaData.data();
        channels = 4;
    }

    const uint32_t bytesPerPixel = channels;
    const uint32_t rowPitch = width * bytesPerPixel;
    const uint32_t alignedRowPitch = (rowPitch + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1)
                                     & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);

    // Create default heap texture resource
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = format;
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12MA::ALLOCATION_DESC allocDesc = {};
    allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

    HRESULT hr = allocator->CreateResource(
        &allocDesc, &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        &m_allocation,
        IID_PPV_ARGS(&m_resource));

    if (FAILED(hr)) {
        SE_LOG_ERROR("Failed to create texture resource ({}x{})", width, height);
        return false;
    }

    // Create upload buffer
    const uint64_t uploadSize = alignedRowPitch * height;

    D3D12_RESOURCE_DESC uploadDesc = {};
    uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadDesc.Width = uploadSize;
    uploadDesc.Height = 1;
    uploadDesc.DepthOrArraySize = 1;
    uploadDesc.MipLevels = 1;
    uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadDesc.SampleDesc.Count = 1;
    uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    D3D12MA::ALLOCATION_DESC uploadAllocDesc = {};
    uploadAllocDesc.HeapType = D3D12_HEAP_TYPE_UPLOAD;

    hr = allocator->CreateResource(
        &uploadAllocDesc, &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        &m_uploadAllocation,
        IID_PPV_ARGS(&m_uploadResource));

    if (FAILED(hr)) {
        SE_LOG_ERROR("Failed to create texture upload buffer");
        return false;
    }

    // Copy pixel data to upload buffer with row-pitch alignment
    void* mapped = nullptr;
    D3D12_RANGE readRange = {0, 0};
    m_uploadResource->Map(0, &readRange, &mapped);

    auto* dst = static_cast<uint8_t*>(mapped);
    for (uint32_t row = 0; row < height; ++row) {
        memcpy(dst + row * alignedRowPitch, srcData + row * rowPitch, rowPitch);
    }

    m_uploadResource->Unmap(0, nullptr);

    // Record copy command
    D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
    srcLoc.pResource = m_uploadResource.Get();
    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLoc.PlacedFootprint.Offset = 0;
    srcLoc.PlacedFootprint.Footprint.Format = format;
    srcLoc.PlacedFootprint.Footprint.Width = width;
    srcLoc.PlacedFootprint.Footprint.Height = height;
    srcLoc.PlacedFootprint.Footprint.Depth = 1;
    srcLoc.PlacedFootprint.Footprint.RowPitch = alignedRowPitch;

    D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
    dstLoc.pResource = m_resource.Get();
    dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLoc.SubresourceIndex = 0;

    cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);

    // Transition to shader resource
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_resource.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
                                  | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);

    // Create SRV
    m_srvHandle = srvHeap.Allocate();
    if (!m_srvHandle.IsValid()) {
        SE_LOG_ERROR("Failed to allocate SRV descriptor for texture");
        return false;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;

    device->CreateShaderResourceView(m_resource.Get(), &srvDesc, m_srvHandle.cpu);

    return true;
}

void Texture::ReleaseUploadResources() {
    m_uploadResource.Reset();
    m_uploadAllocation.Reset();
}

void Texture::Shutdown(DescriptorHeap& srvHeap) {
    if (m_srvHandle.IsValid()) {
        srvHeap.Free(m_srvHandle);
        m_srvHandle = {};
    }
    ReleaseUploadResources();
    m_resource.Reset();
    m_allocation.Reset();
}

} // namespace showcase
