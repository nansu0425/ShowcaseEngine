#pragma once

#include <showcase/demo/IShowcase.h>
#include <showcase/graphics/Buffer.h>
#include <d3d12.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace showcase {

class HelloTriangleShowcase : public IShowcase {
public:
    std::string GetName() const override { return "Hello Triangle"; }
    std::string GetDescription() const override {
        return "Basic DX12 rendering: A colored triangle drawn with vertex colors. "
               "This is the simplest possible GPU rendering demo.";
    }
    std::string GetCategory() const override { return "Rendering"; }

    void OnLoad(RenderContext& ctx) override;
    void OnUnload() override;
    void OnResize(uint32_t width, uint32_t height) override;
    void OnUpdate(float deltaTime) override;
    void OnRender(RenderContext& ctx) override;
    void OnImGui() override;

private:
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    Buffer m_vertexBuffer;

    float m_rotationSpeed = 0.0f;
    float m_currentAngle = 0.0f;
};

} // namespace showcase
