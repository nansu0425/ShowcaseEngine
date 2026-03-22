#pragma once

#include <showcase/core/Assert.h>

#include <d3d12.h>
#include <wrl/client.h>

#include <array>
#include <cstdint>

using Microsoft::WRL::ComPtr;

namespace showcase {

class FrameResource {
public:
    static constexpr uint32_t kNumFrames = 3;

    [[nodiscard]] bool Init(ID3D12Device* device);
    void Shutdown();

    void BeginFrame(uint32_t frameIndex);

    ID3D12CommandAllocator* GetAllocator(uint32_t frameIndex) const {
        SE_ASSERT(frameIndex < kNumFrames);
        return m_allocators[frameIndex].Get();
    }

    uint64_t GetFenceValue(uint32_t frameIndex) const {
        SE_ASSERT(frameIndex < kNumFrames);
        return m_fenceValues[frameIndex];
    }
    void SetFenceValue(uint32_t frameIndex, uint64_t value) {
        SE_ASSERT(frameIndex < kNumFrames);
        m_fenceValues[frameIndex] = value;
    }

private:
    std::array<ComPtr<ID3D12CommandAllocator>, kNumFrames> m_allocators;
    std::array<uint64_t, kNumFrames> m_fenceValues = {};
};

} // namespace showcase
