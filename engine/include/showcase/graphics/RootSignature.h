#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include <vector>

using Microsoft::WRL::ComPtr;

namespace showcase {

struct RootDescriptorDesc {
    uint32_t shaderRegister = 0;
    uint32_t space = 0;
    D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL;
};

struct RootConstantsDesc {
    uint32_t num32BitValues = 0;
    uint32_t shaderRegister = 0;
    uint32_t space = 0;
    D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL;
};

struct DescriptorTableDesc {
    D3D12_DESCRIPTOR_RANGE_TYPE rangeType = {};
    uint32_t numDescriptors = 0;
    uint32_t baseShaderRegister = 0;
    uint32_t space = 0;
    D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL;
};

struct StaticSamplerDesc {
    uint32_t shaderRegister = 0;
    uint32_t space = 0;
    D3D12_FILTER filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL;
};

class RootSignatureBuilder {
public:
    RootSignatureBuilder& AddCBV(const RootDescriptorDesc& desc);
    RootSignatureBuilder& AddSRV(const RootDescriptorDesc& desc);
    RootSignatureBuilder& AddUAV(const RootDescriptorDesc& desc);
    RootSignatureBuilder& AddConstants(const RootConstantsDesc& desc);
    RootSignatureBuilder& AddDescriptorTable(const DescriptorTableDesc& desc);
    RootSignatureBuilder& AddStaticSampler(const StaticSamplerDesc& desc);
    RootSignatureBuilder& SetFlags(D3D12_ROOT_SIGNATURE_FLAGS flags);

    ComPtr<ID3D12RootSignature> Build(ID3D12Device* device);

private:
    std::vector<D3D12_ROOT_PARAMETER1> m_parameters;
    std::vector<D3D12_DESCRIPTOR_RANGE1> m_ranges;
    std::vector<D3D12_STATIC_SAMPLER_DESC> m_staticSamplers;
    D3D12_ROOT_SIGNATURE_FLAGS m_flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
};

} // namespace showcase
