#include <showcase/graphics/SceneRenderer.h>

#include <showcase/core/Log.h>
#include <showcase/core/Profiler.h>
#include <showcase/graphics/CommandList.h>
#include <showcase/graphics/PipelineState.h>
#include <showcase/graphics/RenderContext.h>
#include <showcase/graphics/RootSignature.h>

namespace showcase {

static constexpr int kMaxPointLights = 8;
static constexpr int kMaxSpotLights = 8;

struct GpuPointLight {
    Vector3 position;
    float range;
    Vector3 color;
    float specularPower;
};

struct GpuSpotLight {
    Vector3 position;
    float range; // 16 bytes
    Vector3 direction;
    float innerCos; // 16 bytes
    Vector3 color;
    float outerCos; // 16 bytes
    float specularPower;
    float _pad0[3]; // 16 bytes
}; // Total: 64 bytes

struct alignas(256) PerFrameData {
    Matrix viewProjection;
    Vector3 cameraPosition;
    float _pad0;

    Vector3 lightDirection;
    float lightSpecularPower;
    Vector3 lightColor;
    float _pad1;
    Vector3 ambientColor;
    float _pad2;
    int lightingEnabled;
    int numPointLights;
    int numSpotLights;
    float _pad3;

    GpuPointLight pointLights[kMaxPointLights];
    GpuSpotLight spotLights[kMaxSpotLights];

    // Shadow mapping
    Matrix lightViewProjection;
    int shadowEnabled;
    float shadowBias;
    float shadowMapTexelSize; // 1.0 / shadowMapResolution
    float _pad4;
};

struct alignas(256) ShadowPerFrameData {
    Matrix viewProjection;
};

struct alignas(256) PerObjectData {
    Matrix world;
};

struct alignas(256) PerMaterialData {
    Vector4 baseColorFactor;
    int hasTexture;
    float selectionTint;
    float alphaCutoff;
    int alphaMode;
    float primitiveHighlight;
};

static_assert(sizeof(GpuSpotLight) == 64);
static_assert(sizeof(PerFrameData) <= 1280);
static_assert(sizeof(ShadowPerFrameData) <= 256);
static_assert(sizeof(PerObjectData) <= 256);
static_assert(sizeof(PerMaterialData) <= 256);

// ── Procedural geometry ──────────────────────────────────────────────

void SceneRenderer::CreateCubeModel(RenderContext& ctx, Model& outModel) {
    auto* device = ctx.GetDevice().GetDevice();
    auto* allocator = ctx.GetDevice().GetAllocator();
    ModelVertex vertices[] = {
        // Front face (z = +0.5)
        {{-0.5f, -0.5f, 0.5f}, {0, 0, 1}, {0, 1}},
        {{0.5f, -0.5f, 0.5f}, {0, 0, 1}, {1, 1}},
        {{0.5f, 0.5f, 0.5f}, {0, 0, 1}, {1, 0}},
        {{-0.5f, 0.5f, 0.5f}, {0, 0, 1}, {0, 0}},
        // Back face (z = -0.5)
        {{0.5f, -0.5f, -0.5f}, {0, 0, -1}, {0, 1}},
        {{-0.5f, -0.5f, -0.5f}, {0, 0, -1}, {1, 1}},
        {{-0.5f, 0.5f, -0.5f}, {0, 0, -1}, {1, 0}},
        {{0.5f, 0.5f, -0.5f}, {0, 0, -1}, {0, 0}},
        // Top face (y = +0.5)
        {{-0.5f, 0.5f, 0.5f}, {0, 1, 0}, {0, 1}},
        {{0.5f, 0.5f, 0.5f}, {0, 1, 0}, {1, 1}},
        {{0.5f, 0.5f, -0.5f}, {0, 1, 0}, {1, 0}},
        {{-0.5f, 0.5f, -0.5f}, {0, 1, 0}, {0, 0}},
        // Bottom face (y = -0.5)
        {{-0.5f, -0.5f, -0.5f}, {0, -1, 0}, {0, 1}},
        {{0.5f, -0.5f, -0.5f}, {0, -1, 0}, {1, 1}},
        {{0.5f, -0.5f, 0.5f}, {0, -1, 0}, {1, 0}},
        {{-0.5f, -0.5f, 0.5f}, {0, -1, 0}, {0, 0}},
        // Right face (x = +0.5)
        {{0.5f, -0.5f, 0.5f}, {1, 0, 0}, {0, 1}},
        {{0.5f, -0.5f, -0.5f}, {1, 0, 0}, {1, 1}},
        {{0.5f, 0.5f, -0.5f}, {1, 0, 0}, {1, 0}},
        {{0.5f, 0.5f, 0.5f}, {1, 0, 0}, {0, 0}},
        // Left face (x = -0.5)
        {{-0.5f, -0.5f, -0.5f}, {-1, 0, 0}, {0, 1}},
        {{-0.5f, -0.5f, 0.5f}, {-1, 0, 0}, {1, 1}},
        {{-0.5f, 0.5f, 0.5f}, {-1, 0, 0}, {1, 0}},
        {{-0.5f, 0.5f, -0.5f}, {-1, 0, 0}, {0, 0}},
    };

    uint32_t indices[] = {
        0,  1,  2,  0,  2,  3,  // front
        4,  5,  6,  4,  6,  7,  // back
        8,  9,  10, 8,  10, 11, // top
        12, 13, 14, 12, 14, 15, // bottom
        16, 17, 18, 16, 18, 19, // right
        20, 21, 22, 20, 22, 23, // left
    };

    MeshPrimitive prim;
    if (!prim.vertexBuffer.InitAsVertexBuffer({device, allocator, vertices, sizeof(vertices), sizeof(ModelVertex)})) {
        SE_LOG_ERROR("Failed to create cube vertex buffer");
        return;
    }
    if (!prim.indexBuffer.InitAsIndexBuffer({device, allocator, indices, sizeof(indices)})) {
        SE_LOG_ERROR("Failed to create cube index buffer");
        return;
    }
    prim.indexCount = _countof(indices);
    prim.localAABB = BoundingBox(Vector3(0.f, 0.f, 0.f), Vector3(0.5f, 0.5f, 0.5f));

    Mesh mesh;
    mesh.name = "Cube";
    mesh.primitives.push_back(std::move(prim));

    outModel.meshes.push_back(std::move(mesh));
    outModel.localAABB = BoundingBox(Vector3(0.f, 0.f, 0.f), Vector3(0.5f, 0.5f, 0.5f));
}

void SceneRenderer::CreatePlaneModel(RenderContext& ctx, Model& outModel) {
    auto* device = ctx.GetDevice().GetDevice();
    auto* allocator = ctx.GetDevice().GetAllocator();

    // 1x1 quad in XZ plane, normal +Y, centered at origin
    ModelVertex vertices[] = {
        {{-0.5f, 0.0f, 0.5f}, {0, 1, 0}, {0, 0}},
        {{0.5f, 0.0f, 0.5f}, {0, 1, 0}, {1, 0}},
        {{0.5f, 0.0f, -0.5f}, {0, 1, 0}, {1, 1}},
        {{-0.5f, 0.0f, -0.5f}, {0, 1, 0}, {0, 1}},
    };

    uint32_t indices[] = {0, 1, 2, 0, 2, 3};

    MeshPrimitive prim;
    if (!prim.vertexBuffer.InitAsVertexBuffer({device, allocator, vertices, sizeof(vertices), sizeof(ModelVertex)})) {
        SE_LOG_ERROR("Failed to create plane vertex buffer");
        return;
    }
    if (!prim.indexBuffer.InitAsIndexBuffer({device, allocator, indices, sizeof(indices)})) {
        SE_LOG_ERROR("Failed to create plane index buffer");
        return;
    }
    prim.indexCount = _countof(indices);
    prim.localAABB = BoundingBox(Vector3(0.f, 0.f, 0.f), Vector3(0.5f, 0.001f, 0.5f));

    Mesh mesh;
    mesh.name = "Plane";
    mesh.primitives.push_back(std::move(prim));

    outModel.meshes.push_back(std::move(mesh));
    outModel.localAABB = BoundingBox(Vector3(0.f, 0.f, 0.f), Vector3(0.5f, 0.001f, 0.5f));
}

void SceneRenderer::CreateSphereModel(RenderContext& ctx, Model& outModel) {
    auto* device = ctx.GetDevice().GetDevice();
    auto* allocator = ctx.GetDevice().GetAllocator();

    const uint32_t sectors = 32;
    const uint32_t stacks = 16;
    const float radius = 0.5f;

    std::vector<ModelVertex> vertices;
    std::vector<uint32_t> indices;

    // Generate vertices
    for (uint32_t i = 0; i <= stacks; ++i) {
        float stackAngle = DirectX::XM_PI / 2.0f - DirectX::XM_PI * i / stacks; // from +pi/2 to -pi/2
        float xy = radius * cosf(stackAngle);
        float y = radius * sinf(stackAngle);

        for (uint32_t j = 0; j <= sectors; ++j) {
            float sectorAngle = 2.0f * DirectX::XM_PI * j / sectors;
            float x = xy * cosf(sectorAngle);
            float z = xy * sinf(sectorAngle);

            Vector3 pos(x, y, z);
            Vector3 normal(x, y, z);
            normal.Normalize();
            Vector2 texCoord(static_cast<float>(j) / sectors, static_cast<float>(i) / stacks);

            vertices.push_back({pos, normal, texCoord});
        }
    }

    // Generate indices
    for (uint32_t i = 0; i < stacks; ++i) {
        uint32_t k1 = i * (sectors + 1);
        uint32_t k2 = k1 + sectors + 1;

        for (uint32_t j = 0; j < sectors; ++j, ++k1, ++k2) {
            if (i != 0) {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }
            if (i != (stacks - 1)) {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }

    MeshPrimitive prim;
    const uint32_t sphereVbSize = static_cast<uint32_t>(vertices.size() * sizeof(ModelVertex));
    const uint32_t sphereIbSize = static_cast<uint32_t>(indices.size() * sizeof(uint32_t));
    if (!prim.vertexBuffer.InitAsVertexBuffer(
            {device, allocator, vertices.data(), sphereVbSize, sizeof(ModelVertex)})) {
        SE_LOG_ERROR("Failed to create sphere vertex buffer");
        return;
    }
    if (!prim.indexBuffer.InitAsIndexBuffer({device, allocator, indices.data(), sphereIbSize})) {
        SE_LOG_ERROR("Failed to create sphere index buffer");
        return;
    }
    prim.indexCount = static_cast<uint32_t>(indices.size());
    prim.localAABB = BoundingBox(Vector3(0.f, 0.f, 0.f), Vector3(0.5f, 0.5f, 0.5f));

    Mesh mesh;
    mesh.name = "Sphere";
    mesh.primitives.push_back(std::move(prim));

    outModel.meshes.push_back(std::move(mesh));
    outModel.localAABB = BoundingBox(Vector3(0.f, 0.f, 0.f), Vector3(0.5f, 0.5f, 0.5f));
}

// ── Init / Shutdown ──────────────────────────────────────────────────

void SceneRenderer::Init(RenderContext& ctx) {
    SE_ZONE_SCOPED;
    auto* device = ctx.GetDevice().GetDevice();
    auto* allocator = ctx.GetDevice().GetAllocator();

    // Root signature: CBV b0 (PerFrame), CBV b1 (PerObject), CBV b2 (PerMaterial),
    //                 DescriptorTable(SRV t0), Constants b3 (objectId),
    //                 DescriptorTable(SRV t1 shadowMap), StaticSampler s0, s1
    m_rootSignature =
        RootSignatureBuilder()
            .AddCBV({0})                                                 // slot 0: PerFrame
            .AddCBV({1})                                                 // slot 1: PerObject
            .AddCBV({2})                                                 // slot 2: PerMaterial
            .AddDescriptorTable({D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0}) // slot 3: baseColorTex t0
            .AddConstants({1, 3})                                        // slot 4: objectId (b3, 1 DWORD)
            .AddDescriptorTable({D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1}) // slot 5: shadowMap t1
            .AddStaticSampler({0})                                       // s0: linear wrap
            .AddStaticSampler({1, 0,                                     // s1: comparison sampler
                               D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, D3D12_SHADER_VISIBILITY_PIXEL,
                               D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE})
            .SetFlags(D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)
            .Build(device);

    // Load shaders
    D3D12_SHADER_BYTECODE vs = ctx.GetShaderManager().LoadShader("shaders/mesh_vs_vs.cso");
    D3D12_SHADER_BYTECODE ps = ctx.GetShaderManager().LoadShader("shaders/mesh_ps_ps.cso");

    // PSO
    GraphicsPipelineDesc psoDesc;
    psoDesc.rootSignature = m_rootSignature.Get();
    psoDesc.vertexShader = vs;
    psoDesc.pixelShader = ps;
    psoDesc.inputLayout = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
    psoDesc.rtvFormat = ctx.GetSwapChain().GetFormat();
    psoDesc.dsvFormat = DXGI_FORMAT_D32_FLOAT;

    m_pipelineState = PipelineState::CreateGraphicsPSO(device, psoDesc);

    // Double-sided PSO (no back-face culling)
    {
        GraphicsPipelineDesc doubleSidedDesc = psoDesc;
        doubleSidedDesc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        m_pipelineStateDoubleSided = PipelineState::CreateGraphicsPSO(device, doubleSidedDesc);
    }

    // Object ID pass PSOs
    {
        D3D12_SHADER_BYTECODE idPs = ctx.GetShaderManager().LoadShader("shaders/object_id_ps_ps.cso");
        GraphicsPipelineDesc idDesc = psoDesc;
        idDesc.pixelShader = idPs;
        idDesc.rtvFormat = DXGI_FORMAT_R32_UINT;
        m_objectIdPSO = PipelineState::CreateGraphicsPSO(device, idDesc);

        GraphicsPipelineDesc idDescDS = idDesc;
        idDescDS.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        m_objectIdPSODoubleSided = PipelineState::CreateGraphicsPSO(device, idDescDS);
    }

    // Selection outline pass
    {
        D3D12_SHADER_BYTECODE outlineVs = ctx.GetShaderManager().LoadShader("shaders/outline_vs_vs.cso");
        D3D12_SHADER_BYTECODE outlinePs = ctx.GetShaderManager().LoadShader("shaders/outline_ps_ps.cso");

        // Root signature: slot 0 = root constants (8 DWORDs, b0), slot 1 = SRV descriptor table (t0)
        m_outlineRootSignature =
            RootSignatureBuilder()
                .AddConstants({8, 0}) // 8 x 32-bit values at b0
                .AddDescriptorTable({D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL})
                .SetFlags(D3D12_ROOT_SIGNATURE_FLAG_NONE)
                .Build(device);

        GraphicsPipelineDesc outlineDesc;
        outlineDesc.rootSignature = m_outlineRootSignature.Get();
        outlineDesc.vertexShader = outlineVs;
        outlineDesc.pixelShader = outlinePs;
        outlineDesc.rtvFormat = ctx.GetSwapChain().GetFormat();
        outlineDesc.dsvFormat = DXGI_FORMAT_UNKNOWN;

        // Alpha blending
        outlineDesc.blendState.RenderTarget[0].BlendEnable = TRUE;
        outlineDesc.blendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        outlineDesc.blendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        outlineDesc.blendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        outlineDesc.blendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
        outlineDesc.blendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
        outlineDesc.blendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

        // No backface culling (fullscreen triangle winding)
        outlineDesc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;

        // No depth test/write
        outlineDesc.depthStencilState.DepthEnable = FALSE;
        outlineDesc.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

        m_outlinePSO = PipelineState::CreateGraphicsPSO(device, outlineDesc);
    }

    // Constant buffers (upload heap, 256-byte aligned, per-frame to avoid CPU/GPU race)
    for (uint32_t i = 0; i < FrameResource::kNumFrames; ++i) {
        if (!m_perFrameCB[i].InitAsUploadBuffer(device, allocator, sizeof(PerFrameData)) ||
            !m_perObjectCB[i].InitAsUploadBuffer(device, allocator, sizeof(PerObjectData) * kMaxObjects) ||
            !m_perMaterialCB[i].InitAsUploadBuffer(device, allocator, sizeof(PerMaterialData) * kMaxObjects)) {
            SE_LOG_ERROR("Failed to create constant buffers");
            return;
        }
    }

    // Store device pointers for later use (resize, etc.)
    m_device = device;
    m_allocator = allocator;
    m_srvHeap = &ctx.GetSrvHeap();

    // Create 1x1 white default texture for untextured geometry
    {
        CommandList texCmdList;
        if (!texCmdList.Init(device, D3D12_COMMAND_LIST_TYPE_DIRECT)) {
            SE_LOG_ERROR("Failed to create command list for default texture");
            return;
        }
        texCmdList.Reset();

        uint8_t whitePixel[] = {255, 255, 255, 255};
        if (!m_defaultWhiteTex.InitFromMemory(
                {device, allocator, texCmdList.Get(), &ctx.GetSrvHeap(), whitePixel, 1, 1, 4})) {
            SE_LOG_ERROR("Failed to create default white texture");
            texCmdList.Shutdown();
            return;
        }

        texCmdList.Close();
        ctx.GetDirectQueue().ExecuteCommandList(texCmdList.Get());
        ctx.GetDirectQueue().Flush();
        m_defaultWhiteTex.ReleaseUploadResources();
        texCmdList.Shutdown();
    }

    // Readback buffer for GPU picking (single uint32)
    {
        D3D12_RESOURCE_DESC readbackDesc = {};
        readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        readbackDesc.Width = 256; // D3D12 minimum alignment
        readbackDesc.Height = 1;
        readbackDesc.DepthOrArraySize = 1;
        readbackDesc.MipLevels = 1;
        readbackDesc.Format = DXGI_FORMAT_UNKNOWN;
        readbackDesc.SampleDesc.Count = 1;
        readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_READBACK;

        HRESULT hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &readbackDesc,
                                                     D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                                                     IID_PPV_ARGS(&m_pickReadbackBuffer));
        if (FAILED(hr)) {
            SE_LOG_ERROR("Failed to create pick readback buffer");
            return;
        }
    }

    // Shadow mapping resources
    {
        if (!m_shadowMap.Init({device, allocator, kShadowMapResolution, kShadowMapResolution, DXGI_FORMAT_D32_FLOAT})) {
            SE_LOG_ERROR("Failed to create shadow map depth buffer");
            return;
        }
        m_shadowMap.GetResource()->SetName(L"ShadowMap DepthBuffer");
        m_shadowMap.CreateSRV(device, ctx.GetSrvHeap());

        // Transition shadow map from initial DEPTH_WRITE to shader resource
        // so the first frame's barrier (SRV -> DEPTH_WRITE) is correct
        CommandList initCmdList;
        if (initCmdList.Init(device, D3D12_COMMAND_LIST_TYPE_DIRECT)) {
            initCmdList.Reset();
            initCmdList.TransitionBarrier(m_shadowMap.GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE,
                                          D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
                                              D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            initCmdList.Close();
            ctx.GetDirectQueue().ExecuteCommandList(initCmdList.Get());
            ctx.GetDirectQueue().Flush();
            initCmdList.Shutdown();
        }
        m_shadowMapReady = true;

        // Shadow per-frame constant buffers
        for (uint32_t i = 0; i < FrameResource::kNumFrames; ++i) {
            if (!m_shadowPerFrameCB[i].InitAsUploadBuffer(device, allocator, sizeof(ShadowPerFrameData))) {
                SE_LOG_ERROR("Failed to create shadow per-frame CB");
                return;
            }
        }

        // Shadow PSO (depth-only, no pixel shader)
        GraphicsPipelineDesc shadowPsoDesc;
        shadowPsoDesc.rootSignature = m_rootSignature.Get();
        shadowPsoDesc.vertexShader = vs;
        shadowPsoDesc.pixelShader = {}; // no pixel shader
        shadowPsoDesc.inputLayout = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        };
        shadowPsoDesc.rtvFormat = DXGI_FORMAT_UNKNOWN; // depth-only, no render target
        shadowPsoDesc.dsvFormat = DXGI_FORMAT_D32_FLOAT;
        shadowPsoDesc.rasterizerState.DepthBias = 1500;
        shadowPsoDesc.rasterizerState.SlopeScaledDepthBias = 1.5f;
        shadowPsoDesc.rasterizerState.DepthBiasClamp = 0.0f;
        m_shadowPSO = PipelineState::CreateGraphicsPSO(device, shadowPsoDesc);

        // Shadow PSO double-sided
        GraphicsPipelineDesc shadowPsoDescDS = shadowPsoDesc;
        shadowPsoDescDS.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        m_shadowPSODoubleSided = PipelineState::CreateGraphicsPSO(device, shadowPsoDescDS);
    }

    // Shadow frustum debug visualization
    {
        D3D12_SHADER_BYTECODE frustumVs = ctx.GetShaderManager().LoadShader("shaders/frustum_debug_vs_vs.cso");
        D3D12_SHADER_BYTECODE frustumPs = ctx.GetShaderManager().LoadShader("shaders/frustum_debug_ps_ps.cso");

        m_frustumDebugRootSig = RootSignatureBuilder()
                                    .AddConstants({53, 0}) // 53 DWORDs at b0
                                    .SetFlags(D3D12_ROOT_SIGNATURE_FLAG_NONE)
                                    .Build(device);

        auto makeFrustumDesc = [&](D3D12_PRIMITIVE_TOPOLOGY_TYPE topo) {
            GraphicsPipelineDesc desc;
            desc.rootSignature = m_frustumDebugRootSig.Get();
            desc.vertexShader = frustumVs;
            desc.pixelShader = frustumPs;
            desc.primitiveTopology = topo;
            desc.rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.dsvFormat = DXGI_FORMAT_D32_FLOAT;

            desc.blendState.RenderTarget[0].BlendEnable = TRUE;
            desc.blendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
            desc.blendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
            desc.blendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
            desc.blendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
            desc.blendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
            desc.blendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

            desc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;

            desc.depthStencilState.DepthEnable = TRUE;
            desc.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
            desc.depthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

            return desc;
        };

        m_frustumDebugTriPSO =
            PipelineState::CreateGraphicsPSO(device, makeFrustumDesc(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE));
        m_frustumDebugLinePSO =
            PipelineState::CreateGraphicsPSO(device, makeFrustumDesc(D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE));
    }

    // Shadow map preview (depth-to-grayscale conversion for ImGui display)
    {
        D3D12_SHADER_BYTECODE previewPs = ctx.GetShaderManager().LoadShader("shaders/shadow_debug_ps_ps.cso");

        m_shadowPreviewRootSig =
            RootSignatureBuilder()
                .AddDescriptorTable({D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL})
                .AddStaticSampler({0, 0, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_SHADER_VISIBILITY_PIXEL,
                                   D3D12_TEXTURE_ADDRESS_MODE_CLAMP})
                .SetFlags(D3D12_ROOT_SIGNATURE_FLAG_NONE)
                .Build(device);

        GraphicsPipelineDesc previewDesc;
        previewDesc.rootSignature = m_shadowPreviewRootSig.Get();
        previewDesc.vertexShader =
            ctx.GetShaderManager().LoadShader("shaders/outline_vs_vs.cso"); // fullscreen triangle
        previewDesc.pixelShader = previewPs;
        previewDesc.rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        previewDesc.dsvFormat = DXGI_FORMAT_UNKNOWN;
        previewDesc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        previewDesc.depthStencilState.DepthEnable = FALSE;
        previewDesc.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

        m_shadowPreviewPSO = PipelineState::CreateGraphicsPSO(device, previewDesc);

        float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
        if (!m_shadowPreviewRT.Init({device, allocator, &ctx.GetSrvHeap(), kShadowPreviewSize, kShadowPreviewSize,
                                     DXGI_FORMAT_R8G8B8A8_UNORM, clearColor})) {
            SE_LOG_ERROR("Failed to create shadow preview render target");
            return;
        }
        m_shadowPreviewRT.GetResource()->SetName(L"ShadowPreview RT");
        // RenderTarget::Init() creates resource in PIXEL_SHADER_RESOURCE state,
        // which matches the first RenderShadowPreview() barrier (SRV -> RT). No transition needed.
    }

    SE_LOG_INFO("SceneRenderer initialized");
}

void SceneRenderer::Shutdown() {
    if (m_srvHeap) {
        m_defaultWhiteTex.Shutdown(*m_srvHeap);
        m_objectIdRT.Shutdown(*m_srvHeap);
        m_objectIdDepth.Shutdown(*m_srvHeap);
        m_shadowMap.Shutdown(*m_srvHeap);
        m_shadowPreviewRT.Shutdown(*m_srvHeap);
    }
    m_shadowPreviewPSO.Reset();
    m_shadowPreviewRootSig.Reset();
    m_shadowPSO.Reset();
    m_shadowPSODoubleSided.Reset();
    for (uint32_t i = 0; i < FrameResource::kNumFrames; ++i) {
        m_shadowPerFrameCB[i].Shutdown();
    }
    m_pickReadbackBuffer.Reset();
    m_objectIdPSO.Reset();
    m_objectIdPSODoubleSided.Reset();
    for (uint32_t i = 0; i < FrameResource::kNumFrames; ++i) {
        m_perFrameCB[i].Shutdown();
        m_perObjectCB[i].Shutdown();
        m_perMaterialCB[i].Shutdown();
    }
    m_outlinePSO.Reset();
    m_outlineRootSignature.Reset();
    m_frustumDebugTriPSO.Reset();
    m_frustumDebugLinePSO.Reset();
    m_frustumDebugRootSig.Reset();
    m_pipelineState.Reset();
    m_pipelineStateDoubleSided.Reset();
    m_rootSignature.Reset();
}

void SceneRenderer::OnResize(uint32_t width, uint32_t height) {
    m_aspectRatio = height > 0 ? static_cast<float>(width) / height : 1.0f;

    if (!m_device || !m_srvHeap || width == 0 || height == 0)
        return;

    float idClearColor[] = {0.0f, 0.0f, 0.0f, 0.0f};
    if (m_objectIdRT.GetResource()) {
        m_objectIdRT.Resize({m_device, m_allocator, m_srvHeap, width, height});
    } else {
        if (!m_objectIdRT.Init({m_device, m_allocator, m_srvHeap, width, height, DXGI_FORMAT_R32_UINT, idClearColor})) {
            SE_LOG_ERROR("Failed to init object ID render target");
        }
    }
    m_objectIdRT.GetResource()->SetName(L"ObjectId RT");

    if (m_objectIdDepth.GetResource()) {
        m_objectIdDepth.Resize(m_device, m_allocator, width, height);
    } else {
        if (!m_objectIdDepth.Init({m_device, m_allocator, width, height})) {
            SE_LOG_ERROR("Failed to init object ID depth buffer");
        }
    }
    m_objectIdDepth.GetResource()->SetName(L"ObjectId DepthBuffer");

    m_objectIdNeedsInit = true;
}

// ── Shadow Mapping ──────────────────────────────────────────────────

static constexpr float kMaxShadowDistance = 500.0f; // absolute upper bound (meters)
static constexpr float kMinShadowDistance = 10.0f;  // absolute lower bound to avoid degenerate frustums

static float ComputeAdaptiveShadowDistance(const Camera& camera, const BoundingBox& sceneAABB) {
    Vector3 camPos = camera.GetPosition();
    Vector3 corners[8];
    sceneAABB.GetCorners(corners);

    float maxDistSq = 0.0f;
    for (int i = 0; i < 8; ++i) {
        float distSq = (corners[i] - camPos).LengthSquared();
        maxDistSq = std::max(maxDistSq, distSq);
    }

    float distance = std::sqrt(maxDistSq);
    return std::clamp(distance, kMinShadowDistance, kMaxShadowDistance);
}

struct ShadowFrustumResult {
    Matrix lightViewProj;
    float width;  // maxX - minX
    float height; // maxY - minY
    float nearZ;
    float farZ;
};

static ShadowFrustumResult ComputeDirectionalLightVP(const Vector3& lightDir, const Camera& camera,
                                                     float shadowDistance,
                                                     const std::optional<BoundingBox>& sceneAABB = std::nullopt) {
    // Build a shadow-specific frustum with limited far plane (not the full camera far=1000m).
    // Using the full camera frustum makes the ortho projection too large, destroying depth precision.
    float shadowFar = std::min(camera.GetFarZ(), shadowDistance);
    Matrix shadowProj = PerspectiveFovLH(camera.GetFovY(), camera.GetAspectRatio(), camera.GetNearZ(), shadowFar);
    Matrix shadowVP = camera.GetViewMatrix() * shadowProj;
    Matrix vpInverse = shadowVP.Invert();

    // NDC corners (LH: z in [0,1])
    Vector3 ndcCorners[8] = {
        {-1, -1, 0}, {1, -1, 0}, {-1, 1, 0}, {1, 1, 0}, // near plane
        {-1, -1, 1}, {1, -1, 1}, {-1, 1, 1}, {1, 1, 1}, // far plane
    };

    Vector3 worldCorners[8];
    Vector3 frustumCenter(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 8; ++i) {
        Vector4 worldH =
            Vector4::Transform(Vector4(ndcCorners[i].x, ndcCorners[i].y, ndcCorners[i].z, 1.0f), vpInverse);
        worldCorners[i] = Vector3(worldH.x, worldH.y, worldH.z) / worldH.w;
        frustumCenter += worldCorners[i];
    }
    frustumCenter /= 8.0f;

    // Build light view matrix
    Vector3 up(0.0f, 1.0f, 0.0f);
    // Avoid degenerate case when light is nearly vertical
    if (std::abs(lightDir.Dot(up)) > 0.99f) {
        up = Vector3(1.0f, 0.0f, 0.0f);
    }

    // Place light far enough behind the frustum center to encompass all casters
    float frustumRadius = 0.0f;
    for (int i = 0; i < 8; ++i) {
        frustumRadius = std::max(frustumRadius, (worldCorners[i] - frustumCenter).Length());
    }

    Matrix lightView = LookAtLH(frustumCenter - lightDir * frustumRadius, frustumCenter, up);

    // Transform frustum corners to light space and compute AABB
    float minX = FLT_MAX, maxX = -FLT_MAX;
    float minY = FLT_MAX, maxY = -FLT_MAX;
    float minZ = FLT_MAX, maxZ = -FLT_MAX;
    for (int i = 0; i < 8; ++i) {
        Vector3 lsCorner = Vector3::Transform(worldCorners[i], lightView);
        minX = std::min(minX, lsCorner.x);
        maxX = std::max(maxX, lsCorner.x);
        minY = std::min(minY, lsCorner.y);
        maxY = std::max(maxY, lsCorner.y);
        minZ = std::min(minZ, lsCorner.z);
        maxZ = std::max(maxZ, lsCorner.z);
    }

    // Tighten X/Y bounds by intersecting with scene AABB in light space
    if (sceneAABB.has_value()) {
        Vector3 sceneCorners[8];
        sceneAABB->GetCorners(sceneCorners);

        float sMinX = FLT_MAX, sMaxX = -FLT_MAX;
        float sMinY = FLT_MAX, sMaxY = -FLT_MAX;
        float sMinZ = FLT_MAX, sMaxZ = -FLT_MAX;
        for (int i = 0; i < 8; ++i) {
            Vector3 ls = Vector3::Transform(sceneCorners[i], lightView);
            sMinX = std::min(sMinX, ls.x);
            sMaxX = std::max(sMaxX, ls.x);
            sMinY = std::min(sMinY, ls.y);
            sMaxY = std::max(sMaxY, ls.y);
            sMinZ = std::min(sMinZ, ls.z);
            sMaxZ = std::max(sMaxZ, ls.z);
        }

        // Intersect X/Y (tighten)
        float iMinX = std::max(minX, sMinX);
        float iMaxX = std::min(maxX, sMaxX);
        float iMinY = std::max(minY, sMinY);
        float iMaxY = std::min(maxY, sMaxY);

        if (iMinX < iMaxX && iMinY < iMaxY) {
            minX = iMinX;
            maxX = iMaxX;
            minY = iMinY;
            maxY = iMaxY;
        }
        // Z: use scene AABB range directly — all casters/receivers are within scene AABB
        minZ = sMinZ;
        maxZ = sMaxZ;
    } else {
        // No scene AABB: conservatively extend near plane to capture shadow casters behind camera
        float zRange = maxZ - minZ;
        minZ -= zRange;
    }

    Matrix lightProj = OrthographicOffCenterLH(minX, maxX, minY, maxY, minZ, maxZ);

    ShadowFrustumResult result;
    result.lightViewProj = lightView * lightProj;
    result.width = maxX - minX;
    result.height = maxY - minY;
    result.nearZ = minZ;
    result.farZ = maxZ;
    return result;
}

void SceneRenderer::RenderShadowMap(RenderContext& ctx, Scene& scene, const Matrix& lightViewProj) {
    SE_ZONE_SCOPED_C(profile::kColorRendering);
    auto* cmdList = ctx.GetCommandList().Get();
    uint32_t fi = ctx.GetCurrentFrameIndex();

    // Transition shadow map: SRV -> DEPTH_WRITE
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_shadowMap.GetResource();
    barrier.Transition.StateBefore =
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);

    // Clear shadow depth buffer
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_shadowMap.GetDSV();
    cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Set render target: depth-only (no color RT)
    cmdList->OMSetRenderTargets(0, nullptr, FALSE, &dsv);

    // Viewport and scissor
    D3D12_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(kShadowMapResolution);
    viewport.Height = static_cast<float>(kShadowMapResolution);
    viewport.MaxDepth = 1.0f;
    cmdList->RSSetViewports(1, &viewport);

    D3D12_RECT scissor = {0, 0, static_cast<LONG>(kShadowMapResolution), static_cast<LONG>(kShadowMapResolution)};
    cmdList->RSSetScissorRects(1, &scissor);

    // Set pipeline
    cmdList->SetGraphicsRootSignature(m_rootSignature.Get());
    cmdList->SetPipelineState(m_shadowPSO.Get());
    ID3D12PipelineState* currentPSO = m_shadowPSO.Get();
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Upload shadow per-frame CB (light VP as the viewProjection field)
    ShadowPerFrameData shadowFrameData = {};
    shadowFrameData.viewProjection = lightViewProj;
    m_shadowPerFrameCB[fi].UpdateData(&shadowFrameData, sizeof(shadowFrameData));
    cmdList->SetGraphicsRootConstantBufferView(0, m_shadowPerFrameCB[fi].GetResource()->GetGPUVirtualAddress());

    // Bind dummy SRV for slots 3 and 5 (textures not needed for depth-only)
    cmdList->SetGraphicsRootDescriptorTable(3, m_defaultWhiteTex.GetSRVHandle().gpu);
    cmdList->SetGraphicsRootDescriptorTable(5, m_defaultWhiteTex.GetSRVHandle().gpu);

    // Render all objects (depth only)
    uint32_t objectIndex = 0;
    D3D12_GPU_VIRTUAL_ADDRESS objCbBase = m_perObjectCB[fi].GetResource()->GetGPUVirtualAddress();

    for (const auto& sceneObj : scene.GetObjects()) {
        if (!sceneObj.HasModel())
            continue;

        for (const auto& mesh : sceneObj.modelComp->model->meshes) {
            for (const auto& prim : mesh.primitives) {
                if (prim.indexCount == 0 || objectIndex >= kMaxObjects)
                    continue;

                // Per-object transform
                PerObjectData objData;
                objData.world = sceneObj.worldTransform;
                m_perObjectCB[fi].UpdateDataAtOffset(&objData, sizeof(objData), objectIndex * sizeof(PerObjectData));
                cmdList->SetGraphicsRootConstantBufferView(1, objCbBase + objectIndex * sizeof(PerObjectData));

                // Switch PSO for double-sided materials
                if (prim.material && prim.material->doubleSided) {
                    if (currentPSO != m_shadowPSODoubleSided.Get()) {
                        cmdList->SetPipelineState(m_shadowPSODoubleSided.Get());
                        currentPSO = m_shadowPSODoubleSided.Get();
                    }
                } else if (currentPSO != m_shadowPSO.Get()) {
                    cmdList->SetPipelineState(m_shadowPSO.Get());
                    currentPSO = m_shadowPSO.Get();
                }

                D3D12_VERTEX_BUFFER_VIEW vbView = prim.vertexBuffer.GetVertexBufferView();
                D3D12_INDEX_BUFFER_VIEW ibView = prim.indexBuffer.GetIndexBufferView();
                cmdList->IASetVertexBuffers(0, 1, &vbView);
                cmdList->IASetIndexBuffer(&ibView);
                cmdList->DrawIndexedInstanced(prim.indexCount, 1, 0, 0, 0);
                objectIndex++;
            }
        }
    }

    // Transition shadow map: DEPTH_WRITE -> SRV
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barrier.Transition.StateAfter =
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    cmdList->ResourceBarrier(1, &barrier);
}

// ── Render ───────────────────────────────────────────────────────────

void SceneRenderer::RenderShadowPass(RenderContext& ctx, Camera& camera, Scene& scene) {
    SE_ZONE_SCOPED_C(profile::kColorRendering);
    m_hasShadow = false;
    m_shadowDebugStats = {};
    m_shadowFrustumDebug = {};

    if (!m_shadowMapReady)
        return;

    auto dirLight = scene.GetDirectionalLight();
    if (!dirLight.has_value() || !dirLight->castShadow)
        return;

    // Compute adaptive shadow distance from scene geometry
    float shadowDistance = kMaxShadowDistance;
    auto sceneAABB = scene.GetSceneAABB();
    if (sceneAABB.has_value()) {
        shadowDistance = ComputeAdaptiveShadowDistance(camera, *sceneAABB);
    }

    auto frustum = ComputeDirectionalLightVP(dirLight->direction, camera, shadowDistance, sceneAABB);
    m_cachedLightVP = frustum.lightViewProj;
    m_cachedShadowBias = dirLight->shadowBias;
    RenderShadowMap(ctx, scene, m_cachedLightVP);
    m_hasShadow = true;

    // Populate debug stats
    m_shadowDebugStats.active = true;
    m_shadowDebugStats.lightDirection = dirLight->direction;
    m_shadowDebugStats.frustumWidth = frustum.width;
    m_shadowDebugStats.frustumHeight = frustum.height;
    m_shadowDebugStats.frustumNear = frustum.nearZ;
    m_shadowDebugStats.frustumFar = frustum.farZ;
    m_shadowDebugStats.shadowDistance = shadowDistance;
    m_shadowDebugStats.resolution = kShadowMapResolution;
    m_shadowDebugStats.shadowBias = dirLight->shadowBias;
    m_shadowDebugStats.texelsPerMeter =
        frustum.width > 0.0f ? static_cast<float>(kShadowMapResolution) / frustum.width : 0.0f;

    // Compute world-space frustum corners for debug visualization
    // DX12 LH clip space: z in [0,1]
    const Vector4 ndcCorners[8] = {
        {-1, -1, 0, 1}, {1, -1, 0, 1}, {1, 1, 0, 1}, {-1, 1, 0, 1}, // near
        {-1, -1, 1, 1}, {1, -1, 1, 1}, {1, 1, 1, 1}, {-1, 1, 1, 1}, // far
    };
    Matrix invVP = m_cachedLightVP.Invert();
    for (int i = 0; i < 8; ++i) {
        Vector4 world = Vector4::Transform(ndcCorners[i], invVP);
        m_shadowFrustumDebug.corners[i] = Vector3(world.x, world.y, world.z) / world.w;
    }
    m_shadowFrustumDebug.valid = true;
}

void SceneRenderer::RenderShadowFrustum(RenderContext& ctx, Camera& camera, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                                        D3D12_CPU_DESCRIPTOR_HANDLE dsv, uint32_t width, uint32_t height) {
    if (!m_shadowFrustumDebug.valid)
        return;

    auto* cmdList = ctx.GetCommandList().Get();

    // Re-bind render targets (outline pass may have changed pipeline state)
    cmdList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

    D3D12_VIEWPORT vp = {0, 0, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
    D3D12_RECT scissor = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    cmdList->RSSetViewports(1, &vp);
    cmdList->RSSetScissorRects(1, &scissor);

    cmdList->SetGraphicsRootSignature(m_frustumDebugRootSig.Get());

    // Root constants: VP(16) + color(4) + corners(32) + mode(1) = 53 DWORDs
    struct {
        float viewProjection[16];
        float color[4];
        float corners[32]; // 8 x float4 (xyz + pad)
        uint32_t mode;
    } constants = {};

    Matrix vpMat = camera.GetViewProjectionMatrix();
    memcpy(constants.viewProjection, &vpMat, sizeof(float) * 16);

    for (int i = 0; i < 8; ++i) {
        constants.corners[i * 4 + 0] = m_shadowFrustumDebug.corners[i].x;
        constants.corners[i * 4 + 1] = m_shadowFrustumDebug.corners[i].y;
        constants.corners[i * 4 + 2] = m_shadowFrustumDebug.corners[i].z;
        constants.corners[i * 4 + 3] = 0.0f;
    }

    // Pass 1: Semi-transparent filled faces
    constants.color[0] = 1.0f;
    constants.color[1] = 0.7f;
    constants.color[2] = 0.0f;
    constants.color[3] = 0.12f;
    constants.mode = 0;

    cmdList->SetPipelineState(m_frustumDebugTriPSO.Get());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->SetGraphicsRoot32BitConstants(0, 53, &constants, 0);
    cmdList->DrawInstanced(36, 1, 0, 0);

    // Pass 2: Opaque wireframe edges
    constants.color[0] = 1.0f;
    constants.color[1] = 0.7f;
    constants.color[2] = 0.0f;
    constants.color[3] = 1.0f;
    constants.mode = 1;

    cmdList->SetPipelineState(m_frustumDebugLinePSO.Get());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    cmdList->SetGraphicsRoot32BitConstants(0, 53, &constants, 0);
    cmdList->DrawInstanced(24, 1, 0, 0);
}

void SceneRenderer::RenderShadowPreview(RenderContext& ctx) {
    m_shadowPreviewRendered = false;
    if (!m_hasShadow || !m_shadowPreviewRT.GetResource())
        return;

    auto* cmdList = ctx.GetCommandList().Get();

    // Transition preview RT: SRV -> RENDER_TARGET
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_shadowPreviewRT.GetResource();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);

    // Clear and set render target
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_shadowPreviewRT.GetRTV();
    float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
    cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

    D3D12_VIEWPORT vp = {0,    0,   static_cast<float>(kShadowPreviewSize), static_cast<float>(kShadowPreviewSize),
                         0.0f, 1.0f};
    D3D12_RECT scissor = {0, 0, static_cast<LONG>(kShadowPreviewSize), static_cast<LONG>(kShadowPreviewSize)};
    cmdList->RSSetViewports(1, &vp);
    cmdList->RSSetScissorRects(1, &scissor);

    // Draw fullscreen triangle sampling shadow map depth
    cmdList->SetGraphicsRootSignature(m_shadowPreviewRootSig.Get());
    cmdList->SetPipelineState(m_shadowPreviewPSO.Get());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D12DescriptorHeap* heaps[] = {ctx.GetSrvHeap().GetHeap()};
    cmdList->SetDescriptorHeaps(1, heaps);
    cmdList->SetGraphicsRootDescriptorTable(0, m_shadowMap.GetSRV().gpu);

    cmdList->DrawInstanced(3, 1, 0, 0);

    // Transition preview RT: RENDER_TARGET -> SRV (for ImGui to read)
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    cmdList->ResourceBarrier(1, &barrier);

    m_shadowPreviewRendered = true;
}

D3D12_GPU_DESCRIPTOR_HANDLE SceneRenderer::GetShadowPreviewSRV() const {
    return m_shadowPreviewRT.GetSRVHandle().gpu;
}

void SceneRenderer::Render(RenderContext& ctx, Camera& camera, Scene& scene, int selectedObjectId,
                           const PrimitiveHighlight& highlight) {
    SE_ZONE_SCOPED_C(profile::kColorRendering);
    SE_PLOT("Scene Objects", static_cast<int64_t>(scene.GetObjects().size()));
    auto* cmdList = ctx.GetCommandList().Get();

    // Initialize ObjectId resources after resize so they are valid before any
    // descriptor table binding in this pass.
    if (m_objectIdNeedsInit && m_objectIdRT.GetResource()) {
        m_objectIdNeedsInit = false;

        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = m_objectIdRT.GetResource();
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cmdList->ResourceBarrier(1, &barrier);

        float clearValues[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        cmdList->ClearRenderTargetView(m_objectIdRT.GetRTV(), clearValues, 0, nullptr);
        cmdList->ClearDepthStencilView(m_objectIdDepth.GetDSV(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        cmdList->ResourceBarrier(1, &barrier);
    }

    cmdList->SetGraphicsRootSignature(m_rootSignature.Get());
    cmdList->SetPipelineState(m_pipelineState.Get());
    ID3D12PipelineState* currentPSO = m_pipelineState.Get();
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Bind shadow map SRV (slot 5)
    if (m_hasShadow) {
        cmdList->SetGraphicsRootDescriptorTable(5, m_shadowMap.GetSRV().gpu);
    } else {
        cmdList->SetGraphicsRootDescriptorTable(5, m_defaultWhiteTex.GetSRVHandle().gpu);
    }

    // Update PerFrame CB (use per-frame buffer to avoid CPU/GPU race)
    uint32_t fi = ctx.GetCurrentFrameIndex();
    PerFrameData frameData = {};
    frameData.viewProjection = camera.GetViewProjectionMatrix();
    frameData.cameraPosition = camera.GetPosition();

    auto dirLight = scene.GetDirectionalLight();
    auto ambientLight = scene.GetAmbientLight();

    if (dirLight.has_value() || ambientLight.has_value()) {
        if (dirLight.has_value()) {
            frameData.lightDirection = dirLight->direction;
            frameData.lightColor = dirLight->color;
            frameData.lightSpecularPower = dirLight->specularPower;
        }
        if (ambientLight.has_value()) {
            frameData.ambientColor = ambientLight->color;
        }
        frameData.lightingEnabled = 1;
    }

    std::vector<PointLightData> pointLights = scene.GetPointLights();
    int numPL = std::min(static_cast<int>(pointLights.size()), kMaxPointLights);
    frameData.numPointLights = numPL;
    for (int i = 0; i < numPL; ++i) {
        frameData.pointLights[i].position = pointLights[i].position;
        frameData.pointLights[i].range = pointLights[i].range;
        frameData.pointLights[i].color = pointLights[i].color;
        frameData.pointLights[i].specularPower = pointLights[i].specularPower;
    }
    if (numPL > 0)
        frameData.lightingEnabled = 1;

    std::vector<SpotLightData> spotLights = scene.GetSpotLights();
    int numSL = std::min(static_cast<int>(spotLights.size()), kMaxSpotLights);
    frameData.numSpotLights = numSL;
    for (int i = 0; i < numSL; ++i) {
        frameData.spotLights[i].position = spotLights[i].position;
        frameData.spotLights[i].range = spotLights[i].range;
        frameData.spotLights[i].direction = spotLights[i].direction;
        frameData.spotLights[i].innerCos = spotLights[i].innerCos;
        frameData.spotLights[i].color = spotLights[i].color;
        frameData.spotLights[i].outerCos = spotLights[i].outerCos;
        frameData.spotLights[i].specularPower = spotLights[i].specularPower;
    }
    if (numSL > 0)
        frameData.lightingEnabled = 1;

    // Shadow mapping data
    if (m_hasShadow) {
        frameData.lightViewProjection = m_cachedLightVP;
        frameData.shadowEnabled = 1;
        frameData.shadowBias = m_cachedShadowBias;
        frameData.shadowMapTexelSize = 1.0f / static_cast<float>(kShadowMapResolution);
    }

    m_perFrameCB[fi].UpdateData(&frameData, sizeof(frameData));
    cmdList->SetGraphicsRootConstantBufferView(0, m_perFrameCB[fi].GetResource()->GetGPUVirtualAddress());

    uint32_t objectIndex = 0;
    D3D12_GPU_VIRTUAL_ADDRESS objCbBase = m_perObjectCB[fi].GetResource()->GetGPUVirtualAddress();
    D3D12_GPU_VIRTUAL_ADDRESS matCbBase = m_perMaterialCB[fi].GetResource()->GetGPUVirtualAddress();

    for (const auto& sceneObj : scene.GetObjects()) {
        if (!sceneObj.HasModel())
            continue;

        bool isSelected = (static_cast<int>(sceneObj.id) == selectedObjectId);

        const auto& meshes = sceneObj.modelComp->model->meshes;
        for (size_t mi = 0; mi < meshes.size(); ++mi) {
            const auto& mesh = meshes[mi];
            for (size_t pi = 0; pi < mesh.primitives.size(); ++pi) {
                const auto& prim = mesh.primitives[pi];
                if (prim.indexCount == 0 || objectIndex >= kMaxObjects)
                    continue;

                // Per-object transform
                PerObjectData objData;
                objData.world = sceneObj.worldTransform;
                m_perObjectCB[fi].UpdateDataAtOffset(&objData, sizeof(objData), objectIndex * sizeof(PerObjectData));
                cmdList->SetGraphicsRootConstantBufferView(1, objCbBase + objectIndex * sizeof(PerObjectData));

                // Per-material
                PerMaterialData matData;
                matData.baseColorFactor = Vector4(0.7f, 0.7f, 0.7f, 1.0f);
                matData.hasTexture = 0;
                matData.selectionTint = 0.0f;
                matData.alphaCutoff = 0.5f;
                matData.alphaMode = 0;

                bool isPrimHighlighted = isSelected && static_cast<int>(sceneObj.id) == highlight.objectId &&
                                         static_cast<int>(mi) == highlight.meshIndex &&
                                         static_cast<int>(pi) == highlight.primIndex;
                matData.primitiveHighlight = isPrimHighlighted ? 1.0f : 0.0f;

                if (prim.material) {
                    matData.baseColorFactor = prim.material->baseColorFactor;

                    if (prim.material->alphaMode == AlphaMode::Mask) {
                        matData.alphaMode = 1;
                        matData.alphaCutoff = prim.material->alphaCutoff;
                    }

                    if (prim.material->baseColorTexture) {
                        matData.hasTexture = 1;
                        cmdList->SetGraphicsRootDescriptorTable(3, prim.material->baseColorTexture->GetSRVHandle().gpu);
                    } else {
                        cmdList->SetGraphicsRootDescriptorTable(3, m_defaultWhiteTex.GetSRVHandle().gpu);
                    }

                    // Switch PSO for double-sided materials
                    ID3D12PipelineState* requiredPSO =
                        prim.material->doubleSided ? m_pipelineStateDoubleSided.Get() : m_pipelineState.Get();
                    if (requiredPSO != currentPSO) {
                        cmdList->SetPipelineState(requiredPSO);
                        currentPSO = requiredPSO;
                    }
                } else {
                    cmdList->SetGraphicsRootDescriptorTable(3, m_defaultWhiteTex.GetSRVHandle().gpu);
                    if (currentPSO != m_pipelineState.Get()) {
                        cmdList->SetPipelineState(m_pipelineState.Get());
                        currentPSO = m_pipelineState.Get();
                    }
                }

                // Per-object color override takes priority over material color
                if (sceneObj.modelComp->baseColor.has_value()) {
                    matData.baseColorFactor = *sceneObj.modelComp->baseColor;
                }

                m_perMaterialCB[fi].UpdateDataAtOffset(&matData, sizeof(matData),
                                                       objectIndex * sizeof(PerMaterialData));
                cmdList->SetGraphicsRootConstantBufferView(2, matCbBase + objectIndex * sizeof(PerMaterialData));

                D3D12_VERTEX_BUFFER_VIEW vbView = prim.vertexBuffer.GetVertexBufferView();
                D3D12_INDEX_BUFFER_VIEW ibView = prim.indexBuffer.GetIndexBufferView();
                cmdList->IASetVertexBuffers(0, 1, &vbView);
                cmdList->IASetIndexBuffer(&ibView);
                cmdList->DrawIndexedInstanced(prim.indexCount, 1, 0, 0, 0);
                objectIndex++;
            }
        }
    }

    SE_PLOT("Draw Calls", static_cast<int64_t>(objectIndex));

    // Selection outline pass (uses previous frame's object ID RT).
    // The SRV descriptor table uses PIXEL visibility, so the resource only needs
    // PIXEL_SHADER_RESOURCE — which is already the state from RenderObjectIds().
    if (selectedObjectId >= 0 && m_objectIdRT.GetResource()) {
        cmdList->SetGraphicsRootSignature(m_outlineRootSignature.Get());
        cmdList->SetPipelineState(m_outlinePSO.Get());
        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // Root constants: selectedObjectId, texelSize, pad, outlineColor
        struct {
            uint32_t selectedObjectId;
            uint32_t rtWidth;
            uint32_t rtHeight;
            uint32_t _pad0;
            float outlineColorR;
            float outlineColorG;
            float outlineColorB;
            float outlineColorA;
        } outlineParams;

        outlineParams.selectedObjectId = static_cast<uint32_t>(selectedObjectId);
        outlineParams.rtWidth = m_objectIdRT.GetWidth();
        outlineParams.rtHeight = m_objectIdRT.GetHeight();
        outlineParams._pad0 = 0;
        outlineParams.outlineColorR = 1.0f;
        outlineParams.outlineColorG = 0.55f;
        outlineParams.outlineColorB = 0.0f;
        outlineParams.outlineColorA = 0.8f;

        cmdList->SetGraphicsRoot32BitConstants(0, 8, &outlineParams, 0);
        cmdList->SetGraphicsRootDescriptorTable(1, m_objectIdRT.GetSRVHandle().gpu);
        cmdList->DrawInstanced(3, 1, 0, 0);
    }
}

// ── GPU Object-ID Picking ────────────────────────────────────────────

void SceneRenderer::RenderObjectIds(RenderContext& ctx, Camera& camera, Scene& scene) {
    SE_ZONE_SCOPED_C(profile::kColorRendering);

    if (!m_objectIdRT.GetResource())
        return;

    auto* cmdList = ctx.GetCommandList().Get();

    // Transition ID RT to render target
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_objectIdRT.GetResource();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);

    // Clear ID RT to 0 (no object)
    float clearValues[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_objectIdRT.GetRTV();
    cmdList->ClearRenderTargetView(rtvHandle, clearValues, 0, nullptr);

    // Clear depth
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_objectIdDepth.GetDSV();
    cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Set render target
    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    // Set viewport and scissor to match ID RT dimensions
    D3D12_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(m_objectIdRT.GetWidth());
    viewport.Height = static_cast<float>(m_objectIdRT.GetHeight());
    viewport.MaxDepth = 1.0f;
    cmdList->RSSetViewports(1, &viewport);

    D3D12_RECT scissor = {0, 0, static_cast<LONG>(m_objectIdRT.GetWidth()),
                          static_cast<LONG>(m_objectIdRT.GetHeight())};
    cmdList->RSSetScissorRects(1, &scissor);

    // Set pipeline state
    cmdList->SetGraphicsRootSignature(m_rootSignature.Get());
    cmdList->SetPipelineState(m_objectIdPSO.Get());
    ID3D12PipelineState* currentPSO = m_objectIdPSO.Get();
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Bind dummy SRV for shadow map slot (slot 5, required by root signature)
    cmdList->SetGraphicsRootDescriptorTable(5, m_defaultWhiteTex.GetSRVHandle().gpu);

    // Reuse PerFrame CB (already uploaded by Render())
    uint32_t fi = ctx.GetCurrentFrameIndex();
    cmdList->SetGraphicsRootConstantBufferView(0, m_perFrameCB[fi].GetResource()->GetGPUVirtualAddress());

    uint32_t objectIndex = 0;
    D3D12_GPU_VIRTUAL_ADDRESS objCbBase = m_perObjectCB[fi].GetResource()->GetGPUVirtualAddress();
    D3D12_GPU_VIRTUAL_ADDRESS matCbBase = m_perMaterialCB[fi].GetResource()->GetGPUVirtualAddress();

    for (const auto& sceneObj : scene.GetObjects()) {
        if (!sceneObj.HasModel())
            continue;

        for (const auto& mesh : sceneObj.modelComp->model->meshes) {
            for (const auto& prim : mesh.primitives) {
                if (prim.indexCount == 0 || objectIndex >= kMaxObjects)
                    continue;

                // Reuse per-object and per-material CBs (already uploaded by Render())
                cmdList->SetGraphicsRootConstantBufferView(1, objCbBase + objectIndex * sizeof(PerObjectData));
                cmdList->SetGraphicsRootConstantBufferView(2, matCbBase + objectIndex * sizeof(PerMaterialData));

                // Set texture for alpha testing
                if (prim.material && prim.material->baseColorTexture) {
                    cmdList->SetGraphicsRootDescriptorTable(3, prim.material->baseColorTexture->GetSRVHandle().gpu);
                } else {
                    cmdList->SetGraphicsRootDescriptorTable(3, m_defaultWhiteTex.GetSRVHandle().gpu);
                }

                // Set object ID via root constant (slot 4)
                uint32_t objId = sceneObj.id;
                cmdList->SetGraphicsRoot32BitConstant(4, objId, 0);

                // Switch PSO for double-sided materials
                if (prim.material) {
                    ID3D12PipelineState* requiredPSO =
                        prim.material->doubleSided ? m_objectIdPSODoubleSided.Get() : m_objectIdPSO.Get();
                    if (requiredPSO != currentPSO) {
                        cmdList->SetPipelineState(requiredPSO);
                        currentPSO = requiredPSO;
                    }
                } else if (currentPSO != m_objectIdPSO.Get()) {
                    cmdList->SetPipelineState(m_objectIdPSO.Get());
                    currentPSO = m_objectIdPSO.Get();
                }

                D3D12_VERTEX_BUFFER_VIEW vbView = prim.vertexBuffer.GetVertexBufferView();
                D3D12_INDEX_BUFFER_VIEW ibView = prim.indexBuffer.GetIndexBufferView();
                cmdList->IASetVertexBuffers(0, 1, &vbView);
                cmdList->IASetIndexBuffer(&ibView);
                cmdList->DrawIndexedInstanced(prim.indexCount, 1, 0, 0, 0);
                objectIndex++;
            }
        }
    }

    // If a pick was requested, copy the target pixel to the readback buffer
    if (m_pickRequested) {
        m_pickRequested = false;

        // Transition to copy source
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        cmdList->ResourceBarrier(1, &barrier);

        // Copy 1x1 pixel region
        D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
        srcLoc.pResource = m_objectIdRT.GetResource();
        srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        srcLoc.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
        dstLoc.pResource = m_pickReadbackBuffer.Get();
        dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dstLoc.PlacedFootprint.Offset = 0;
        dstLoc.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R32_UINT;
        dstLoc.PlacedFootprint.Footprint.Width = 1;
        dstLoc.PlacedFootprint.Footprint.Height = 1;
        dstLoc.PlacedFootprint.Footprint.Depth = 1;
        dstLoc.PlacedFootprint.Footprint.RowPitch = 256; // D3D12 minimum alignment

        D3D12_BOX srcBox = {};
        srcBox.left = static_cast<UINT>(m_pickX);
        srcBox.top = static_cast<UINT>(m_pickY);
        srcBox.right = srcBox.left + 1;
        srcBox.bottom = srcBox.top + 1;
        srcBox.front = 0;
        srcBox.back = 1;

        cmdList->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, &srcBox);

        // Transition back to SRV
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        cmdList->ResourceBarrier(1, &barrier);

        // Record fence value — result will be available after this frame's GPU work completes
        m_pickFenceValue = ctx.GetDirectQueue().GetCurrentFenceValue() + 1;
        m_pickReady = true;
    } else {
        // Transition back to SRV
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        cmdList->ResourceBarrier(1, &barrier);
    }
}

void SceneRenderer::RequestPick(int pixelX, int pixelY) {
    m_pickX = pixelX;
    m_pickY = pixelY;
    m_pickRequested = true;
    m_pickReady = false;
}

bool SceneRenderer::IsPickComplete(RenderContext& ctx) const {
    return m_pickReady && ctx.GetDirectQueue().IsFenceComplete(m_pickFenceValue);
}

int SceneRenderer::GetPickResult(RenderContext& ctx) {
    if (!IsPickComplete(ctx))
        return -1;

    m_pickReady = false;

    uint32_t* data = nullptr;
    D3D12_RANGE readRange = {0, sizeof(uint32_t)};
    HRESULT hr = m_pickReadbackBuffer->Map(0, &readRange, reinterpret_cast<void**>(&data));
    if (FAILED(hr))
        return -1;

    uint32_t pickedId = *data;

    D3D12_RANGE writeRange = {0, 0};
    m_pickReadbackBuffer->Unmap(0, &writeRange);

    m_lastPickResult = (pickedId == 0) ? -1 : static_cast<int>(pickedId);
    return m_lastPickResult;
}

} // namespace showcase
