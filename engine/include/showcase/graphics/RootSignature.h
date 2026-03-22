#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include <vector>

using Microsoft::WRL::ComPtr;

namespace showcase {

class RootSignatureBuilder {
public:
    RootSignatureBuilder& AddCBV(uint32_t shaderRegister, uint32_t space = 0,
                                  D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);
    RootSignatureBuilder& AddSRV(uint32_t shaderRegister, uint32_t space = 0,
                                  D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);
    RootSignatureBuilder& AddUAV(uint32_t shaderRegister, uint32_t space = 0,
                                  D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);
    RootSignatureBuilder& AddConstants(uint32_t num32BitValues, uint32_t shaderRegister,
                                        uint32_t space = 0,
                                        D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);
    RootSignatureBuilder& AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE rangeType,
                                              uint32_t numDescriptors,
                                              uint32_t baseShaderRegister, uint32_t space = 0,
                                              D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);
    RootSignatureBuilder& AddStaticSampler(uint32_t shaderRegister, uint32_t space = 0,
                                            D3D12_FILTER filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR,
                                            D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);
    RootSignatureBuilder& SetFlags(D3D12_ROOT_SIGNATURE_FLAGS flags);

    ComPtr<ID3D12RootSignature> Build(ID3D12Device* device);

private:
    std::vector<D3D12_ROOT_PARAMETER1> m_parameters;
    std::vector<D3D12_DESCRIPTOR_RANGE1> m_ranges;
    std::vector<D3D12_STATIC_SAMPLER_DESC> m_staticSamplers;
    D3D12_ROOT_SIGNATURE_FLAGS m_flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
};

} // namespace showcase
