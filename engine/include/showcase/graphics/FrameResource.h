#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include <array>

using Microsoft::WRL::ComPtr;

namespace showcase {

class FrameResource {
public:
    static constexpr uint32_t NUM_FRAMES = 3;

    bool Init(ID3D12Device* device);
    void Shutdown();

    void BeginFrame(uint32_t frameIndex);

    ID3D12CommandAllocator* GetAllocator(uint32_t frameIndex) const {
        return m_allocators[frameIndex].Get();
    }

    uint64_t GetFenceValue(uint32_t frameIndex) const { return m_fenceValues[frameIndex]; }
    void SetFenceValue(uint32_t frameIndex, uint64_t value) { m_fenceValues[frameIndex] = value; }

private:
    std::array<ComPtr<ID3D12CommandAllocator>, NUM_FRAMES> m_allocators;
    std::array<uint64_t, NUM_FRAMES> m_fenceValues = {};
};

} // namespace showcase
