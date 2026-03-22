#include <showcase/graphics/FrameResource.h>
#include <showcase/core/Assert.h>
#include <showcase/core/Log.h>

namespace showcase {

bool FrameResource::Init(ID3D12Device* device) {
    for (uint32_t i = 0; i < kNumFrames; i++) {
        HRESULT hr = device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&m_allocators[i]));
        if (FAILED(hr)) {
            SE_LOG_ERROR("Failed to create command allocator for frame {}", i);
            return false;
        }
        m_fenceValues[i] = 0;
    }
    return true;
}

void FrameResource::Shutdown() {
    for (auto& alloc : m_allocators) {
        alloc.Reset();
    }
}

void FrameResource::BeginFrame(uint32_t frameIndex) {
    SE_ASSERT(frameIndex < kNumFrames);
    m_allocators[frameIndex]->Reset();
}

} // namespace showcase
