#include <showcase/graphics/PipelineState.h>
#include <showcase/core/Log.h>

namespace showcase {

// ── Graphics PSO ─────────────────────────────────────────────────────
ComPtr<ID3D12PipelineState> PipelineState::CreateGraphicsPSO(ID3D12Device* device,
                                                               const GraphicsPipelineDesc& desc) {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = desc.rootSignature;
    psoDesc.VS = desc.vertexShader;
    psoDesc.PS = desc.pixelShader;
    psoDesc.BlendState = desc.blendState;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.RasterizerState = desc.rasterizerState;
    psoDesc.DepthStencilState = desc.depthStencilState;
    psoDesc.InputLayout = {
        desc.inputLayout.data(),
        static_cast<UINT>(desc.inputLayout.size())
    };
    psoDesc.PrimitiveTopologyType = desc.primitiveTopology;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = desc.rtvFormat;
    psoDesc.DSVFormat = desc.dsvFormat;
    psoDesc.SampleDesc = {desc.sampleCount, 0};

    ComPtr<ID3D12PipelineState> pso;
    HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));
    if (FAILED(hr)) {
        SE_LOG_ERROR("Failed to create graphics pipeline state (HRESULT: 0x{:08X})", static_cast<uint32_t>(hr));
        return nullptr;
    }

    return pso;
}

// ── Compute PSO ──────────────────────────────────────────────────────
ComPtr<ID3D12PipelineState> PipelineState::CreateComputePSO(ID3D12Device* device,
                                                              ID3D12RootSignature* rootSignature,
                                                              D3D12_SHADER_BYTECODE computeShader) {
    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature;
    psoDesc.CS = computeShader;

    ComPtr<ID3D12PipelineState> pso;
    HRESULT hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pso));
    if (FAILED(hr)) {
        SE_LOG_ERROR("Failed to create compute pipeline state (HRESULT: 0x{:08X})", static_cast<uint32_t>(hr));
        return nullptr;
    }

    return pso;
}

} // namespace showcase
