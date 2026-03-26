#include <showcase/graphics/RootSignature.h>

#include <showcase/core/Log.h>

namespace showcase {

// ── Root parameters ──────────────────────────────────────────────────
RootSignatureBuilder& RootSignatureBuilder::AddCBV(const RootDescriptorDesc& desc) {
    D3D12_ROOT_PARAMETER1 param = {};
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    param.Descriptor.ShaderRegister = desc.shaderRegister;
    param.Descriptor.RegisterSpace = desc.space;
    param.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
    param.ShaderVisibility = desc.visibility;
    m_parameters.push_back(param);
    return *this;
}

RootSignatureBuilder& RootSignatureBuilder::AddSRV(const RootDescriptorDesc& desc) {
    D3D12_ROOT_PARAMETER1 param = {};
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    param.Descriptor.ShaderRegister = desc.shaderRegister;
    param.Descriptor.RegisterSpace = desc.space;
    param.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
    param.ShaderVisibility = desc.visibility;
    m_parameters.push_back(param);
    return *this;
}

RootSignatureBuilder& RootSignatureBuilder::AddUAV(const RootDescriptorDesc& desc) {
    D3D12_ROOT_PARAMETER1 param = {};
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    param.Descriptor.ShaderRegister = desc.shaderRegister;
    param.Descriptor.RegisterSpace = desc.space;
    param.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
    param.ShaderVisibility = desc.visibility;
    m_parameters.push_back(param);
    return *this;
}

RootSignatureBuilder& RootSignatureBuilder::AddConstants(const RootConstantsDesc& desc) {
    D3D12_ROOT_PARAMETER1 param = {};
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    param.Constants.ShaderRegister = desc.shaderRegister;
    param.Constants.RegisterSpace = desc.space;
    param.Constants.Num32BitValues = desc.num32BitValues;
    param.ShaderVisibility = desc.visibility;
    m_parameters.push_back(param);
    return *this;
}

RootSignatureBuilder& RootSignatureBuilder::AddDescriptorTable(const DescriptorTableDesc& desc) {
    D3D12_DESCRIPTOR_RANGE1 range = {};
    range.RangeType = desc.rangeType;
    range.NumDescriptors = desc.numDescriptors;
    range.BaseShaderRegister = desc.baseShaderRegister;
    range.RegisterSpace = desc.space;
    range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    m_ranges.push_back(range);

    D3D12_ROOT_PARAMETER1 param = {};
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    param.DescriptorTable.NumDescriptorRanges = 1;
    // Will be patched in Build() to point to the correct range
    param.DescriptorTable.pDescriptorRanges = reinterpret_cast<const D3D12_DESCRIPTOR_RANGE1*>(m_ranges.size() - 1);
    param.ShaderVisibility = desc.visibility;
    m_parameters.push_back(param);
    return *this;
}

// ── Samplers / Flags ─────────────────────────────────────────────────
RootSignatureBuilder& RootSignatureBuilder::AddStaticSampler(const StaticSamplerDesc& desc) {
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = desc.filter;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.MipLODBias = 0.0f;
    sampler.MaxAnisotropy = 16;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = desc.shaderRegister;
    sampler.RegisterSpace = desc.space;
    sampler.ShaderVisibility = desc.visibility;
    m_staticSamplers.push_back(sampler);
    return *this;
}

RootSignatureBuilder& RootSignatureBuilder::SetFlags(D3D12_ROOT_SIGNATURE_FLAGS flags) {
    m_flags = flags;
    return *this;
}

// ── Build ────────────────────────────────────────────────────────────
ComPtr<ID3D12RootSignature> RootSignatureBuilder::Build(ID3D12Device* device) {
    // Patch descriptor table pointers
    for (auto& param : m_parameters) {
        if (param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
            size_t rangeIndex = reinterpret_cast<size_t>(param.DescriptorTable.pDescriptorRanges);
            param.DescriptorTable.pDescriptorRanges = &m_ranges[rangeIndex];
        }
    }

    D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc = {};
    desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    desc.Desc_1_1.NumParameters = static_cast<UINT>(m_parameters.size());
    desc.Desc_1_1.pParameters = m_parameters.empty() ? nullptr : m_parameters.data();
    desc.Desc_1_1.NumStaticSamplers = static_cast<UINT>(m_staticSamplers.size());
    desc.Desc_1_1.pStaticSamplers = m_staticSamplers.empty() ? nullptr : m_staticSamplers.data();
    desc.Desc_1_1.Flags = m_flags;

    ComPtr<ID3DBlob> serializedRootSig;
    ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3D12SerializeVersionedRootSignature(&desc, &serializedRootSig, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            SE_LOG_ERROR("Root signature serialization error: {}",
                         static_cast<const char*>(errorBlob->GetBufferPointer()));
        }
        return nullptr;
    }

    ComPtr<ID3D12RootSignature> rootSignature;
    hr = device->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(),
                                     IID_PPV_ARGS(&rootSignature));
    if (FAILED(hr)) {
        SE_LOG_ERROR("Failed to create root signature");
        return nullptr;
    }

    return rootSignature;
}

} // namespace showcase
