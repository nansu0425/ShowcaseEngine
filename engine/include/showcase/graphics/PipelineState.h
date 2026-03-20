#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <cstdint>

using Microsoft::WRL::ComPtr;

namespace showcase {

// Default D3D12 state helpers
inline D3D12_RASTERIZER_DESC DefaultRasterizerDesc() {
    D3D12_RASTERIZER_DESC desc = {};
    desc.FillMode = D3D12_FILL_MODE_SOLID;
    desc.CullMode = D3D12_CULL_MODE_BACK;
    desc.FrontCounterClockwise = FALSE;
    desc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    desc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    desc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    desc.DepthClipEnable = TRUE;
    desc.MultisampleEnable = FALSE;
    desc.AntialiasedLineEnable = FALSE;
    desc.ForcedSampleCount = 0;
    desc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    return desc;
}

inline D3D12_BLEND_DESC DefaultBlendDesc() {
    D3D12_BLEND_DESC desc = {};
    desc.AlphaToCoverageEnable = FALSE;
    desc.IndependentBlendEnable = FALSE;
    for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
        desc.RenderTarget[i].BlendEnable = FALSE;
        desc.RenderTarget[i].LogicOpEnable = FALSE;
        desc.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
        desc.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
        desc.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
        desc.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
        desc.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
        desc.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        desc.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
        desc.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }
    return desc;
}

inline D3D12_DEPTH_STENCIL_DESC DefaultDepthStencilDesc() {
    D3D12_DEPTH_STENCIL_DESC desc = {};
    desc.DepthEnable = TRUE;
    desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    desc.StencilEnable = FALSE;
    desc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
    desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
    desc.FrontFace = {D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP,
                      D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS};
    desc.BackFace = {D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP,
                     D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS};
    return desc;
}

struct GraphicsPipelineDesc {
    ID3D12RootSignature* rootSignature = nullptr;
    D3D12_SHADER_BYTECODE vertexShader = {};
    D3D12_SHADER_BYTECODE pixelShader = {};
    std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout;
    D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveTopology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    DXGI_FORMAT rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT dsvFormat = DXGI_FORMAT_UNKNOWN;
    D3D12_RASTERIZER_DESC rasterizerState = DefaultRasterizerDesc();
    D3D12_BLEND_DESC blendState = DefaultBlendDesc();
    D3D12_DEPTH_STENCIL_DESC depthStencilState = DefaultDepthStencilDesc();
    uint32_t sampleCount = 1;
};

class PipelineState {
public:
    static ComPtr<ID3D12PipelineState> CreateGraphicsPSO(ID3D12Device* device,
                                                          const GraphicsPipelineDesc& desc);

    static ComPtr<ID3D12PipelineState> CreateComputePSO(ID3D12Device* device,
                                                         ID3D12RootSignature* rootSignature,
                                                         D3D12_SHADER_BYTECODE computeShader);
};

} // namespace showcase
