#pragma once

#include <showcase/graphics/Device.h>
#include <showcase/graphics/CommandQueue.h>
#include <showcase/graphics/SwapChain.h>
#include <showcase/graphics/DescriptorHeap.h>
#include <showcase/graphics/FrameResource.h>
#include <showcase/graphics/ShaderManager.h>
#include <showcase/graphics/CommandList.h>

namespace showcase {

class RenderContext {
public:
    bool Init(HWND hwnd, uint32_t width, uint32_t height);
    void Shutdown();

    void BeginFrame();
    void EndFrame();
    void Resize(uint32_t width, uint32_t height);

    Device& GetDevice() { return m_device; }
    CommandQueue& GetDirectQueue() { return m_directQueue; }
    SwapChain& GetSwapChain() { return m_swapChain; }
    DescriptorHeap& GetSrvHeap() { return m_srvHeap; }
    FrameResource& GetFrameResource() { return m_frameResource; }
    ShaderManager& GetShaderManager() { return m_shaderManager; }
    CommandList& GetCommandList() { return m_commandList; }
    uint32_t GetCurrentFrameIndex() const { return m_currentFrameIndex; }

private:
    Device m_device;
    CommandQueue m_directQueue;
    SwapChain m_swapChain;
    DescriptorHeap m_srvHeap;
    FrameResource m_frameResource;
    ShaderManager m_shaderManager;
    CommandList m_commandList;

    uint32_t m_currentFrameIndex = 0;
};

} // namespace showcase
