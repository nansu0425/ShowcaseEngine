#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>

using Microsoft::WRL::ComPtr;

namespace showcase {

class CommandQueue {
public:
    [[nodiscard]] bool Init(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type);
    void Shutdown();

    uint64_t ExecuteCommandList(ID3D12CommandList* commandList);
    uint64_t Signal();
    void WaitForFenceValue(uint64_t fenceValue);
    void Flush();
    bool IsFenceComplete(uint64_t fenceValue) const;

    ID3D12CommandQueue* GetQueue() const { return m_queue.Get(); }
    uint64_t GetCurrentFenceValue() const { return m_fenceValue; }

private:
    ComPtr<ID3D12CommandQueue> m_queue;
    ComPtr<ID3D12Fence> m_fence;
    HANDLE m_fenceEvent = nullptr;
    uint64_t m_fenceValue = 0;
};

} // namespace showcase
