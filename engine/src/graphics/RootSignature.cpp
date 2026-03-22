#include <showcase/graphics/RootSignature.h>
#include <showcase/core/Log.h>

namespace showcase {

RootSignatureBuilder& RootSignatureBuilder::AddCBV(uint32_t shaderRegister, uint32_t space,
                                                     D3D12_SHADER_VISIBILITY visibility) {
    D3D12_ROOT_PARAMETER1 param = {};
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    param.Descriptor.ShaderRegister = shaderRegister;
    param.Descriptor.RegisterSpace = space;
    param.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
    param.ShaderVisibility = visibility;
    m_parameters.push_back(param);
    return *this;
}

RootSignatureBuilder& RootSignatureBuilder::AddSRV(uint32_t shaderRegister, uint32_t space,
                                                     D3D12_SHADER_VISIBILITY visibility) {
    D3D12_ROOT_PARAMETER1 param = {};
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    param.Descriptor.ShaderRegister = shaderRegister;
    param.Descriptor.RegisterSpace = space;
    param.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
    param.ShaderVisibility = visibility;
    m_parameters.push_back(param);
    return *this;
}

RootSignatureBuilder& RootSignatureBuilder::AddUAV(uint32_t shaderRegister, uint32_t space,
                                                     D3D12_SHADER_VISIBILITY visibility) {
    D3D12_ROOT_PARAMETER1 param = {};
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
    param.Descriptor.ShaderRegister = shaderRegister;
    param.Descriptor.RegisterSpace = space;
    param.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
    param.ShaderVisibility = visibility;
    m_parameters.push_back(param);
    return *this;
}

RootSignatureBuilder& RootSignatureBuilder::AddConstants(uint32_t num32BitValues,
                                                           uint32_t shaderRegister,
                                                           uint32_t space,
                                                           D3D12_SHADER_VISIBILITY visibility) {
    D3D12_ROOT_PARAMETER1 param = {};
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    param.Constants.ShaderRegister = shaderRegister;
    param.Constants.RegisterSpace = space;
    param.Constants.Num32BitValues = num32BitValues;
    param.ShaderVisibility = visibility;
    m_parameters.push_back(param);
    return *this;
}

RootSignatureBuilder& RootSignatureBuilder::AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE rangeType,
                                                                 uint32_t numDescriptors,
                                                                 uint32_t baseShaderRegister,
                                                                 uint32_t space,
                                                                 D3D12_SHADER_VISIBILITY visibility) {
    D3D12_DESCRIPTOR_RANGE1 range = {};
    range.RangeType = rangeType;
    range.NumDescriptors = numDescriptors;
    range.BaseShaderRegister = baseShaderRegister;
    range.RegisterSpace = space;
    range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    m_ranges.push_back(range);

    D3D12_ROOT_PARAMETER1 param = {};
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    param.DescriptorTable.NumDescriptorRanges = 1;
    // Will be patched in Build() to point to the correct range
    param.DescriptorTable.pDescriptorRanges = reinterpret_cast<const D3D12_DESCRIPTOR_RANGE1*>(m_ranges.size() - 1);
    param.ShaderVisibility = visibility;
    m_parameters.push_back(param);
    return *this;
}

RootSignatureBuilder& RootSignatureBuilder::AddStaticSampler(uint32_t shaderRegister, uint32_t space,
                                                               D3D12_FILTER filter,
                                                               D3D12_SHADER_VISIBILITY visibility) {
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = filter;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.MipLODBias = 0.0f;
    sampler.MaxAnisotropy = 16;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = shaderRegister;
    sampler.RegisterSpace = space;
    sampler.ShaderVisibility = visibility;
    m_staticSamplers.push_back(sampler);
    return *this;
}

RootSignatureBuilder& RootSignatureBuilder::SetFlags(D3D12_ROOT_SIGNATURE_FLAGS flags) {
    m_flags = flags;
    return *this;
}

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
    hr = device->CreateRootSignature(0, serializedRootSig->GetBufferPointer(),
                                      serializedRootSig->GetBufferSize(),
                                      IID_PPV_ARGS(&rootSignature));
    if (FAILED(hr)) {
        SE_LOG_ERROR("Failed to create root signature");
        return nullptr;
    }

    return rootSignature;
}

} // namespace showcase
