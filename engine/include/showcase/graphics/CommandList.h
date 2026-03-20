#pragma once

#include <d3d12.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace showcase {

class CommandList {
public:
    bool Init(ID3D12Device* device, D3D12_COMMAND_LIST_TYPE type);
    void Shutdown();

    void Reset();
    void Close();

    void TransitionBarrier(ID3D12Resource* resource,
                           D3D12_RESOURCE_STATES before,
                           D3D12_RESOURCE_STATES after);

    ID3D12GraphicsCommandList* Get() const { return m_commandList.Get(); }
    ID3D12GraphicsCommandList* operator->() const { return m_commandList.Get(); }

private:
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    ComPtr<ID3D12CommandAllocator> m_allocator;
    D3D12_COMMAND_LIST_TYPE m_type = D3D12_COMMAND_LIST_TYPE_DIRECT;
};

} // namespace showcase
