#include <showcase/graphics/SceneRenderer.h>

#include <showcase/core/Log.h>
#include <showcase/core/Profiler.h>
#include <showcase/graphics/CommandList.h>
#include <showcase/graphics/PipelineState.h>
#include <showcase/graphics/RenderContext.h>
#include <showcase/graphics/RootSignature.h>

namespace showcase {

static constexpr int kMaxPointLights = 6;
static constexpr int kMaxSpotLights = 8;

struct GpuPointLight {
    Vector3 position;
    float range; // 16 bytes
    Vector3 color;
    float specularPower; // 16 bytes
    int shadowIndex;     // -1 = no shadow, 0..5 = cubemap index
    float shadowBias;
    float _pad[2]; // 16 bytes
}; // Total: 48 bytes

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

    // Shadow mapping (directional)
    Matrix lightViewProjection;
    int shadowEnabled;
    float shadowBias;
    float shadowMapTexelSize; // 1.0 / shadowMapResolution
    float _pad4;

    // Point light shadow mapping
    int numPointShadowLights;
    float pointShadowNearZ;
    float _pad5[2];
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

static_assert(sizeof(GpuPointLight) == 48);
static_assert(sizeof(GpuSpotLight) == 64);
static_assert(sizeof(PerFrameData) <= 1536);
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
    //                 DescriptorTable(SRV t1 shadowMap),
    //                 DescriptorTable(SRV t2..t7 pointShadowMaps), StaticSampler s0, s1
    m_rootSignature =
        RootSignatureBuilder()
            .AddCBV({0})                                                 // slot 0: PerFrame
            .AddCBV({1})                                                 // slot 1: PerObject
            .AddCBV({2})                                                 // slot 2: PerMaterial
            .AddDescriptorTable({D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0}) // slot 3: baseColorTex t0
            .AddConstants({1, 3})                                        // slot 4: objectId (b3, 1 DWORD)
            .AddDescriptorTable({D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1}) // slot 5: shadowMap t1
            .AddDescriptorTable({D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2}) // slot 6: pointShadowMap0 t2
            .AddDescriptorTable({D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 3}) // slot 7: pointShadowMap1 t3
            .AddDescriptorTable({D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4}) // slot 8: pointShadowMap2 t4
            .AddDescriptorTable({D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5}) // slot 9: pointShadowMap3 t5
            .AddDescriptorTable({D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6}) // slot 10: pointShadowMap4 t6
            .AddDescriptorTable({D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 7}) // slot 11: pointShadowMap5 t7
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
            uint32_t shadowCBSize = kMaxShadowCBSlots * sizeof(ShadowPerFrameData);
            if (!m_shadowPerFrameCB[i].InitAsUploadBuffer(device, allocator, shadowCBSize)) {
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

    // Point light shadow cubemaps
    {
        CommandList initCmdList;
        bool cmdListReady = initCmdList.Init(device, D3D12_COMMAND_LIST_TYPE_DIRECT);
        if (cmdListReady)
            initCmdList.Reset();

        for (int i = 0; i < kMaxPointLights; ++i) {
            if (!m_pointShadowMaps[i].Init(device, allocator, kPointShadowMapResolution)) {
                SE_LOG_ERROR("Failed to create point shadow cubemap {}", i);
                return;
            }
            std::wstring name = L"PointShadowMap" + std::to_wstring(i);
            m_pointShadowMaps[i].GetResource()->SetName(name.c_str());
            m_pointShadowMaps[i].CreateSRV(device, ctx.GetSrvHeap());

            if (cmdListReady) {
                // Clear all 6 faces to initialize the depth/stencil resource
                // (D3D12 requires Clear/Discard before first use of DS resources)
                for (uint32_t face = 0; face < CubemapDepthBuffer::kNumFaces; ++face) {
                    initCmdList.Get()->ClearDepthStencilView(m_pointShadowMaps[i].GetFaceDSV(face),
                                                             D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
                }
                initCmdList.TransitionBarrier(m_pointShadowMaps[i].GetResource(), D3D12_RESOURCE_STATE_DEPTH_WRITE,
                                              D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE |
                                                  D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            }
        }

        if (cmdListReady) {
            initCmdList.Close();
            ctx.GetDirectQueue().ExecuteCommandList(initCmdList.Get());
            ctx.GetDirectQueue().Flush();
            initCmdList.Shutdown();
        }
        m_pointShadowMapsReady = true;
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

    // Point light gizmo (depth-tested wireframe rings)
    {
        D3D12_SHADER_BYTECODE gizmoVs = ctx.GetShaderManager().LoadShader("shaders/light_gizmo_vs_vs.cso");
        D3D12_SHADER_BYTECODE gizmoPs = ctx.GetShaderManager().LoadShader("shaders/frustum_debug_ps_ps.cso");

        m_lightGizmoRootSig = RootSignatureBuilder()
                                  .AddConstants({28, 0}) // 28 DWORDs at b0
                                  .SetFlags(D3D12_ROOT_SIGNATURE_FLAG_NONE)
                                  .Build(device);

        GraphicsPipelineDesc desc;
        desc.rootSignature = m_lightGizmoRootSig.Get();
        desc.vertexShader = gizmoVs;
        desc.pixelShader = gizmoPs;
        desc.primitiveTopology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
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

        m_lightGizmoLinePSO = PipelineState::CreateGraphicsPSO(device, desc);
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

    // Shadow coverage overlay (fullscreen pass: reconstruct world pos from depth, recompute shadow)
    {
        D3D12_SHADER_BYTECODE overlayPs = ctx.GetShaderManager().LoadShader("shaders/shadow_overlay_ps_ps.cso");

        m_shadowOverlayRootSig =
            RootSignatureBuilder()
                .AddConstants({36, 0})                                   // slot 0: 36 DWORDs at b0
                .AddDescriptorTable({D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, // slot 1: scene depth t0
                                     0, 0, D3D12_SHADER_VISIBILITY_PIXEL})
                .AddDescriptorTable({D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, // slot 2: shadow map t1
                                     1, 0, D3D12_SHADER_VISIBILITY_PIXEL})
                .AddStaticSampler({0, 0, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_SHADER_VISIBILITY_PIXEL,
                                   D3D12_TEXTURE_ADDRESS_MODE_CLAMP}) // s0: point clamp
                .AddStaticSampler({1, 0,                              // s1: comparison sampler
                                   D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, D3D12_SHADER_VISIBILITY_PIXEL,
                                   D3D12_TEXTURE_ADDRESS_MODE_BORDER, D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE})
                .SetFlags(D3D12_ROOT_SIGNATURE_FLAG_NONE)
                .Build(device);

        GraphicsPipelineDesc overlayDesc;
        overlayDesc.rootSignature = m_shadowOverlayRootSig.Get();
        overlayDesc.vertexShader = ctx.GetShaderManager().LoadShader("shaders/outline_vs_vs.cso");
        overlayDesc.pixelShader = overlayPs;
        overlayDesc.rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        overlayDesc.dsvFormat = DXGI_FORMAT_UNKNOWN;

        // Alpha blending (same as outline pass)
        overlayDesc.blendState.RenderTarget[0].BlendEnable = TRUE;
        overlayDesc.blendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        overlayDesc.blendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        overlayDesc.blendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        overlayDesc.blendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
        overlayDesc.blendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
        overlayDesc.blendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

        overlayDesc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        overlayDesc.depthStencilState.DepthEnable = FALSE;
        overlayDesc.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

        m_shadowOverlayPSO = PipelineState::CreateGraphicsPSO(device, overlayDesc);
    }

    // Point shadow coverage overlay (fullscreen pass: reconstruct world pos, sample cubemap)
    {
        D3D12_SHADER_BYTECODE overlayPs = ctx.GetShaderManager().LoadShader("shaders/point_shadow_overlay_ps_ps.cso");

        m_pointShadowOverlayRootSig =
            RootSignatureBuilder()
                .AddConstants({24, 0})                                   // slot 0: 24 DWORDs at b0
                .AddDescriptorTable({D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, // slot 1: scene depth t0
                                     0, 0, D3D12_SHADER_VISIBILITY_PIXEL})
                .AddDescriptorTable({D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, // slot 2: cubemap t1
                                     1, 0, D3D12_SHADER_VISIBILITY_PIXEL})
                .AddStaticSampler({0, 0, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_SHADER_VISIBILITY_PIXEL,
                                   D3D12_TEXTURE_ADDRESS_MODE_CLAMP}) // s0: point clamp (depth)
                .AddStaticSampler({1, 0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_SHADER_VISIBILITY_PIXEL,
                                   D3D12_TEXTURE_ADDRESS_MODE_CLAMP}) // s1: linear clamp (cubemap)
                .SetFlags(D3D12_ROOT_SIGNATURE_FLAG_NONE)
                .Build(device);

        GraphicsPipelineDesc pointOverlayDesc;
        pointOverlayDesc.rootSignature = m_pointShadowOverlayRootSig.Get();
        pointOverlayDesc.vertexShader = ctx.GetShaderManager().LoadShader("shaders/outline_vs_vs.cso");
        pointOverlayDesc.pixelShader = overlayPs;
        pointOverlayDesc.rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        pointOverlayDesc.dsvFormat = DXGI_FORMAT_UNKNOWN;

        pointOverlayDesc.blendState.RenderTarget[0].BlendEnable = TRUE;
        pointOverlayDesc.blendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        pointOverlayDesc.blendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        pointOverlayDesc.blendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        pointOverlayDesc.blendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
        pointOverlayDesc.blendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
        pointOverlayDesc.blendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

        pointOverlayDesc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        pointOverlayDesc.depthStencilState.DepthEnable = FALSE;
        pointOverlayDesc.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

        m_pointShadowOverlayPSO = PipelineState::CreateGraphicsPSO(device, pointOverlayDesc);
    }

    // Cubemap face ID overlay (fullscreen pass: reconstruct world pos, dominant axis → face color)
    {
        D3D12_SHADER_BYTECODE faceOverlayPs =
            ctx.GetShaderManager().LoadShader("shaders/point_shadow_face_overlay_ps_ps.cso");

        m_cubemapFaceOverlayRootSig =
            RootSignatureBuilder()
                .AddConstants({24, 0})                                   // slot 0: 24 DWORDs at b0
                .AddDescriptorTable({D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, // slot 1: scene depth t0
                                     0, 0, D3D12_SHADER_VISIBILITY_PIXEL})
                .AddStaticSampler({0, 0, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_SHADER_VISIBILITY_PIXEL,
                                   D3D12_TEXTURE_ADDRESS_MODE_CLAMP}) // s0: point clamp (depth)
                .SetFlags(D3D12_ROOT_SIGNATURE_FLAG_NONE)
                .Build(device);

        GraphicsPipelineDesc faceOverlayDesc;
        faceOverlayDesc.rootSignature = m_cubemapFaceOverlayRootSig.Get();
        faceOverlayDesc.vertexShader = ctx.GetShaderManager().LoadShader("shaders/outline_vs_vs.cso");
        faceOverlayDesc.pixelShader = faceOverlayPs;
        faceOverlayDesc.rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        faceOverlayDesc.dsvFormat = DXGI_FORMAT_UNKNOWN;

        faceOverlayDesc.blendState.RenderTarget[0].BlendEnable = TRUE;
        faceOverlayDesc.blendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        faceOverlayDesc.blendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        faceOverlayDesc.blendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        faceOverlayDesc.blendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
        faceOverlayDesc.blendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
        faceOverlayDesc.blendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

        faceOverlayDesc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        faceOverlayDesc.depthStencilState.DepthEnable = FALSE;
        faceOverlayDesc.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

        m_cubemapFaceOverlayPSO = PipelineState::CreateGraphicsPSO(device, faceOverlayDesc);
    }

    // Depth precision heatmap overlay (shares root signature with cubemap face overlay)
    {
        D3D12_SHADER_BYTECODE heatmapPs =
            ctx.GetShaderManager().LoadShader("shaders/point_shadow_depth_heatmap_ps_ps.cso");

        GraphicsPipelineDesc heatmapDesc;
        heatmapDesc.rootSignature = m_cubemapFaceOverlayRootSig.Get();
        heatmapDesc.vertexShader = ctx.GetShaderManager().LoadShader("shaders/outline_vs_vs.cso");
        heatmapDesc.pixelShader = heatmapPs;
        heatmapDesc.rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        heatmapDesc.dsvFormat = DXGI_FORMAT_UNKNOWN;

        heatmapDesc.blendState.RenderTarget[0].BlendEnable = TRUE;
        heatmapDesc.blendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        heatmapDesc.blendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        heatmapDesc.blendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        heatmapDesc.blendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
        heatmapDesc.blendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
        heatmapDesc.blendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

        heatmapDesc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        heatmapDesc.depthStencilState.DepthEnable = FALSE;
        heatmapDesc.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

        m_depthHeatmapOverlayPSO = PipelineState::CreateGraphicsPSO(device, heatmapDesc);
    }

    // Cubemap face preview (point shadow depth-to-grayscale for inspector cross layout)
    {
        D3D12_SHADER_BYTECODE cubemapPreviewPs = ctx.GetShaderManager().LoadShader("shaders/cubemap_preview_ps_ps.cso");

        m_cubemapPreviewRootSig =
            RootSignatureBuilder()
                .AddConstants({4, 0})                                    // slot 0: 4 DWORDs at b0
                .AddDescriptorTable({D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, // slot 1: cubemap SRV t0
                                     0, 0, D3D12_SHADER_VISIBILITY_PIXEL})
                .AddStaticSampler({0, 0, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_SHADER_VISIBILITY_PIXEL,
                                   D3D12_TEXTURE_ADDRESS_MODE_CLAMP})
                .SetFlags(D3D12_ROOT_SIGNATURE_FLAG_NONE)
                .Build(device);

        GraphicsPipelineDesc previewDesc;
        previewDesc.rootSignature = m_cubemapPreviewRootSig.Get();
        previewDesc.vertexShader = ctx.GetShaderManager().LoadShader("shaders/outline_vs_vs.cso");
        previewDesc.pixelShader = cubemapPreviewPs;
        previewDesc.rtvFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        previewDesc.dsvFormat = DXGI_FORMAT_UNKNOWN;
        previewDesc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        previewDesc.depthStencilState.DepthEnable = FALSE;
        previewDesc.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

        m_cubemapPreviewPSO = PipelineState::CreateGraphicsPSO(device, previewDesc);

        float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
        for (uint32_t i = 0; i < 6; ++i) {
            if (!m_cubemapPreviewRT[i].Init({device, allocator, &ctx.GetSrvHeap(), kCubemapPreviewFaceSize,
                                             kCubemapPreviewFaceSize, DXGI_FORMAT_R8G8B8A8_UNORM, clearColor})) {
                SE_LOG_ERROR("Failed to create cubemap preview RT face {}", i);
                return;
            }
            std::wstring name = L"CubemapPreview RT Face " + std::to_wstring(i);
            m_cubemapPreviewRT[i].GetResource()->SetName(name.c_str());
        }
    }

    SE_LOG_INFO("SceneRenderer initialized");
}

void SceneRenderer::Shutdown() {
    if (m_srvHeap) {
        m_defaultWhiteTex.Shutdown(*m_srvHeap);
        m_objectIdRT.Shutdown(*m_srvHeap);
        m_objectIdDepth.Shutdown(*m_srvHeap);
        m_shadowMap.Shutdown(*m_srvHeap);
        for (int i = 0; i < kMaxPointLights; ++i)
            m_pointShadowMaps[i].Shutdown(*m_srvHeap);
        m_shadowPreviewRT.Shutdown(*m_srvHeap);
        for (int i = 0; i < 6; ++i)
            m_cubemapPreviewRT[i].Shutdown(*m_srvHeap);
    }
    m_cubemapPreviewPSO.Reset();
    m_cubemapPreviewRootSig.Reset();
    m_shadowOverlayPSO.Reset();
    m_shadowOverlayRootSig.Reset();
    m_pointShadowOverlayPSO.Reset();
    m_pointShadowOverlayRootSig.Reset();
    m_depthHeatmapOverlayPSO.Reset();
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
    m_lightGizmoLinePSO.Reset();
    m_lightGizmoRootSig.Reset();
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

void SceneRenderer::RenderDepthOnlyObjects(RenderContext& ctx, Scene& scene, const Matrix& viewProj,
                                           D3D12_CPU_DESCRIPTOR_HANDLE dsv, uint32_t resolution,
                                           uint32_t shadowCBSlot) {
    auto* cmdList = ctx.GetCommandList().Get();
    uint32_t fi = ctx.GetCurrentFrameIndex();

    // Clear and bind DSV
    cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
    cmdList->OMSetRenderTargets(0, nullptr, FALSE, &dsv);

    // Viewport and scissor
    D3D12_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(resolution);
    viewport.Height = static_cast<float>(resolution);
    viewport.MaxDepth = 1.0f;
    cmdList->RSSetViewports(1, &viewport);

    D3D12_RECT scissor = {0, 0, static_cast<LONG>(resolution), static_cast<LONG>(resolution)};
    cmdList->RSSetScissorRects(1, &scissor);

    // Set pipeline
    cmdList->SetGraphicsRootSignature(m_rootSignature.Get());
    cmdList->SetPipelineState(m_shadowPSO.Get());
    ID3D12PipelineState* currentPSO = m_shadowPSO.Get();
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Upload shadow per-frame CB (view-projection matrix) at unique offset per face
    ShadowPerFrameData shadowFrameData = {};
    shadowFrameData.viewProjection = viewProj;
    uint32_t cbOffset = shadowCBSlot * sizeof(ShadowPerFrameData);
    m_shadowPerFrameCB[fi].UpdateDataAtOffset(&shadowFrameData, sizeof(shadowFrameData), cbOffset);
    cmdList->SetGraphicsRootConstantBufferView(0,
                                               m_shadowPerFrameCB[fi].GetResource()->GetGPUVirtualAddress() + cbOffset);

    // Bind dummy SRV for unused texture slots (depth-only pass)
    cmdList->SetGraphicsRootDescriptorTable(3, m_defaultWhiteTex.GetSRVHandle().gpu);
    cmdList->SetGraphicsRootDescriptorTable(5, m_defaultWhiteTex.GetSRVHandle().gpu);
    for (int i = 0; i < kMaxPointLights; ++i)
        cmdList->SetGraphicsRootDescriptorTable(6 + i, m_defaultWhiteTex.GetSRVHandle().gpu);

    // Render all objects (depth only)
    uint32_t objectIndex = 0;
    D3D12_GPU_VIRTUAL_ADDRESS objCbBase = m_perObjectCB[fi].GetResource()->GetGPUVirtualAddress();

    for (const auto& sceneObj : scene.GetObjects()) {
        if (!sceneObj.HasModel())
            continue;
        if (!sceneObj.enabled)
            continue;

        for (const auto& mesh : sceneObj.modelComp->model->meshes) {
            for (const auto& prim : mesh.primitives) {
                if (prim.indexCount == 0 || objectIndex >= kMaxObjects)
                    continue;

                PerObjectData objData;
                objData.world = sceneObj.worldTransform;
                m_perObjectCB[fi].UpdateDataAtOffset(&objData, sizeof(objData), objectIndex * sizeof(PerObjectData));
                cmdList->SetGraphicsRootConstantBufferView(1, objCbBase + objectIndex * sizeof(PerObjectData));

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
}

void SceneRenderer::RenderShadowMap(RenderContext& ctx, Scene& scene, const Matrix& lightViewProj) {
    SE_ZONE_SCOPED_C(profile::kColorRendering);
    auto* cmdList = ctx.GetCommandList().Get();

    // Transition shadow map: SRV -> DEPTH_WRITE
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_shadowMap.GetResource();
    barrier.Transition.StateBefore =
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);

    RenderDepthOnlyObjects(ctx, scene, lightViewProj, m_shadowMap.GetDSV(), kShadowMapResolution, 0);

    // Transition shadow map: DEPTH_WRITE -> SRV
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barrier.Transition.StateAfter =
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    cmdList->ResourceBarrier(1, &barrier);
}

void SceneRenderer::RenderPointShadowPass(RenderContext& ctx, Scene& scene) {
    SE_ZONE_SCOPED_C(profile::kColorRendering);
    m_numPointShadowLights = 0;

    if (!m_pointShadowMapsReady)
        return;

    auto pointLights = scene.GetPointLights();
    auto* cmdList = ctx.GetCommandList().Get();

    // Cubemap face directions (DX cubemap order: +X, -X, +Y, -Y, +Z, -Z)
    struct FaceInfo {
        Vector3 dir;
        Vector3 up;
    };
    static const FaceInfo faces[6] = {
        {{1, 0, 0}, {0, 1, 0}},  // +X
        {{-1, 0, 0}, {0, 1, 0}}, // -X
        {{0, 1, 0}, {0, 0, -1}}, // +Y
        {{0, -1, 0}, {0, 0, 1}}, // -Y
        {{0, 0, 1}, {0, 1, 0}},  // +Z
        {{0, 0, -1}, {0, 1, 0}}, // -Z
    };

    for (const auto& pl : pointLights) {
        if (!pl.castShadow || m_numPointShadowLights >= kMaxPointLights)
            continue;

        int idx = m_numPointShadowLights;
        m_pointShadowPositions[idx] = pl.position;
        m_pointShadowRanges[idx] = pl.range;
        m_pointShadowBiases[idx] = pl.shadowBias;

        // Transition cubemap: SRV -> DEPTH_WRITE
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = m_pointShadowMaps[idx].GetResource();
        barrier.Transition.StateBefore =
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cmdList->ResourceBarrier(1, &barrier);

        // 90-degree perspective projection (aspect=1, near=0.1, far=range)
        Matrix proj = PerspectiveFovLH(kPiOver2, 1.0f, kPointShadowNearZ, pl.range);

        for (int face = 0; face < 6; ++face) {
            Vector3 target = pl.position + faces[face].dir;
            Matrix view = LookAtLH(pl.position, target, faces[face].up);
            Matrix viewProj = view * proj;

            // Slot 0 is directional; point lights start at slot 1
            uint32_t cbSlot = 1 + idx * 6 + face;
            D3D12_CPU_DESCRIPTOR_HANDLE faceDSV = m_pointShadowMaps[idx].GetFaceDSV(face);
            RenderDepthOnlyObjects(ctx, scene, viewProj, faceDSV, kPointShadowMapResolution, cbSlot);
        }

        // Transition cubemap: DEPTH_WRITE -> SRV
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        barrier.Transition.StateAfter =
            D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        cmdList->ResourceBarrier(1, &barrier);

        m_numPointShadowLights++;
    }
}

int SceneRenderer::GetPointShadowIndex(int objectId) const {
    for (int i = 0; i < m_numCachedPointShadowEntries; ++i) {
        if (m_cachedPointShadowEntries[i].objectId == objectId)
            return m_cachedPointShadowEntries[i].shadowIndex;
    }
    return -1;
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

void SceneRenderer::SetPointLightGizmo(const Vector3& center, float range) {
    m_pointLightGizmo = {center, range, true};
}

void SceneRenderer::ClearPointLightGizmo() {
    m_pointLightGizmo.valid = false;
}

void SceneRenderer::RenderPointLightGizmo(RenderContext& ctx, Camera& camera, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                                          D3D12_CPU_DESCRIPTOR_HANDLE dsv, uint32_t width, uint32_t height) {
    if (!m_pointLightGizmo.valid)
        return;

    auto* cmdList = ctx.GetCommandList().Get();

    cmdList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

    D3D12_VIEWPORT vp = {0, 0, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
    D3D12_RECT scissor = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    cmdList->RSSetViewports(1, &vp);
    cmdList->RSSetScissorRects(1, &scissor);

    cmdList->SetGraphicsRootSignature(m_lightGizmoRootSig.Get());

    // Root constants: VP(16) + color(4) + centerAndRange(4) + viewport(2) + lineWidth(1) + pad(1) = 28 DWORDs
    struct {
        float viewProjection[16];
        float color[4];
        float centerAndRange[4];
        float viewportSize[2];
        float lineWidthPixels;
        float _pad;
    } constants = {};

    Matrix vpMat = camera.GetViewProjectionMatrix();
    memcpy(constants.viewProjection, &vpMat, sizeof(float) * 16);

    constants.color[0] = 0.39f; // R (100/255)
    constants.color[1] = 0.78f; // G (200/255)
    constants.color[2] = 1.0f;  // B (255/255)
    constants.color[3] = 0.47f; // A (120/255)

    constants.centerAndRange[0] = m_pointLightGizmo.center.x;
    constants.centerAndRange[1] = m_pointLightGizmo.center.y;
    constants.centerAndRange[2] = m_pointLightGizmo.center.z;
    constants.centerAndRange[3] = m_pointLightGizmo.range;

    constants.viewportSize[0] = static_cast<float>(width);
    constants.viewportSize[1] = static_cast<float>(height);
    constants.lineWidthPixels = 2.0f;

    cmdList->SetPipelineState(m_lightGizmoLinePSO.Get());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->SetGraphicsRoot32BitConstants(0, 28, &constants, 0);
    cmdList->DrawInstanced(432, 1, 0, 0); // 3 rings x 24 segments x 6 verts/quad
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

void SceneRenderer::RenderCubemapPreview(RenderContext& ctx, int shadowIndex) {
    SE_ZONE_SCOPED_C(profile::kColorRendering);
    m_cubemapPreviewRendered = false;

    if (shadowIndex < 0 || shadowIndex >= m_numPointShadowLights)
        return;
    if (!m_cubemapPreviewRT[0].GetResource())
        return;

    auto* cmdList = ctx.GetCommandList().Get();

    // Batch-transition all 6 preview RTs: SRV -> RENDER_TARGET
    D3D12_RESOURCE_BARRIER barriers[6] = {};
    for (uint32_t i = 0; i < 6; ++i) {
        barriers[i].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[i].Transition.pResource = m_cubemapPreviewRT[i].GetResource();
        barriers[i].Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barriers[i].Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barriers[i].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    }
    cmdList->ResourceBarrier(6, barriers);

    // Set shared state once
    cmdList->SetGraphicsRootSignature(m_cubemapPreviewRootSig.Get());
    cmdList->SetPipelineState(m_cubemapPreviewPSO.Get());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ID3D12DescriptorHeap* heaps[] = {ctx.GetSrvHeap().GetHeap()};
    cmdList->SetDescriptorHeaps(1, heaps);
    cmdList->SetGraphicsRootDescriptorTable(1, m_pointShadowMaps[shadowIndex].GetSRV().gpu);

    D3D12_VIEWPORT vp = {
        0, 0, static_cast<float>(kCubemapPreviewFaceSize), static_cast<float>(kCubemapPreviewFaceSize), 0.0f, 1.0f};
    D3D12_RECT scissor = {0, 0, static_cast<LONG>(kCubemapPreviewFaceSize), static_cast<LONG>(kCubemapPreviewFaceSize)};
    cmdList->RSSetViewports(1, &vp);
    cmdList->RSSetScissorRects(1, &scissor);

    // Render each face
    struct CubemapPreviewConstants {
        uint32_t faceIndex;
        float nearZ;
        float farZ;
        uint32_t _pad;
    };

    for (uint32_t face = 0; face < 6; ++face) {
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_cubemapPreviewRT[face].GetRTV();
        float clearColor[] = {0.0f, 0.0f, 0.0f, 1.0f};
        cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
        cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

        CubemapPreviewConstants constants;
        constants.faceIndex = face;
        constants.nearZ = kPointShadowNearZ;
        constants.farZ = m_pointShadowRanges[shadowIndex];
        constants._pad = 0;
        cmdList->SetGraphicsRoot32BitConstants(0, 4, &constants, 0);

        cmdList->DrawInstanced(3, 1, 0, 0);
    }

    // Batch-transition all 6 RTs back: RENDER_TARGET -> SRV
    for (uint32_t i = 0; i < 6; ++i) {
        barriers[i].Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barriers[i].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }
    cmdList->ResourceBarrier(6, barriers);

    m_cubemapPreviewRendered = true;
}

D3D12_GPU_DESCRIPTOR_HANDLE SceneRenderer::GetCubemapPreviewFaceSRV(uint32_t face) const {
    return m_cubemapPreviewRT[face].GetSRVHandle().gpu;
}

void SceneRenderer::RenderShadowOverlay(RenderContext& ctx, Camera& camera, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                                        DepthBuffer& sceneDepthBuffer, uint32_t width, uint32_t height) {
    if (!m_hasShadow)
        return;

    auto* cmdList = ctx.GetCommandList().Get();

    // Transition scene depth buffer: DEPTH_WRITE -> PIXEL_SHADER_RESOURCE
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = sceneDepthBuffer.GetResource();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barrier.Transition.StateAfter =
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    cmdList->ResourceBarrier(1, &barrier);

    // Bind RTV only (no depth test needed)
    cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

    D3D12_VIEWPORT viewport = {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
    D3D12_RECT scissor = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    cmdList->RSSetViewports(1, &viewport);
    cmdList->RSSetScissorRects(1, &scissor);

    cmdList->SetGraphicsRootSignature(m_shadowOverlayRootSig.Get());
    cmdList->SetPipelineState(m_shadowOverlayPSO.Get());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Root constants: invViewProjection, lightViewProjection, shadow params
    Matrix vp = camera.GetViewMatrix() * camera.GetProjectionMatrix();
    Matrix invVP = vp.Invert();

    struct ShadowOverlayParams {
        Matrix invViewProjection;
        Matrix lightViewProjection;
        float shadowBias;
        float shadowMapTexelSize;
        float overlayAlpha;
        float _pad0;
    } params;
    params.invViewProjection = invVP;
    params.lightViewProjection = m_cachedLightVP;
    params.shadowBias = m_cachedShadowBias;
    params.shadowMapTexelSize = 1.0f / static_cast<float>(kShadowMapResolution);
    params.overlayAlpha = 0.35f;
    params._pad0 = 0.0f;

    cmdList->SetGraphicsRoot32BitConstants(0, 36, &params, 0);

    ID3D12DescriptorHeap* heaps[] = {ctx.GetSrvHeap().GetHeap()};
    cmdList->SetDescriptorHeaps(1, heaps);
    cmdList->SetGraphicsRootDescriptorTable(1, sceneDepthBuffer.GetSRV().gpu);
    cmdList->SetGraphicsRootDescriptorTable(2, m_shadowMap.GetSRV().gpu);

    cmdList->DrawInstanced(3, 1, 0, 0);

    // Transition scene depth buffer back: PIXEL_SHADER_RESOURCE -> DEPTH_WRITE
    barrier.Transition.StateBefore =
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    cmdList->ResourceBarrier(1, &barrier);
}

void SceneRenderer::RenderPointShadowOverlay(RenderContext& ctx, Camera& camera, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                                             DepthBuffer& sceneDepthBuffer, uint32_t width, uint32_t height,
                                             int shadowIndex) {
    if (shadowIndex < 0 || shadowIndex >= m_numPointShadowLights)
        return;

    auto* cmdList = ctx.GetCommandList().Get();

    // Transition scene depth buffer: DEPTH_WRITE -> PIXEL_SHADER_RESOURCE
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = sceneDepthBuffer.GetResource();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barrier.Transition.StateAfter =
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    cmdList->ResourceBarrier(1, &barrier);

    // Bind RTV only (no depth test needed)
    cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

    D3D12_VIEWPORT viewport = {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
    D3D12_RECT scissor = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    cmdList->RSSetViewports(1, &viewport);
    cmdList->RSSetScissorRects(1, &scissor);

    cmdList->SetGraphicsRootSignature(m_pointShadowOverlayRootSig.Get());
    cmdList->SetPipelineState(m_pointShadowOverlayPSO.Get());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Root constants: invViewProjection + point light params
    Matrix vp = camera.GetViewMatrix() * camera.GetProjectionMatrix();
    Matrix invVP = vp.Invert();

    struct PointShadowOverlayParams {
        Matrix invViewProjection;
        float lightPosition[3];
        float lightRange;
        float nearZ;
        float shadowBias;
        float overlayAlpha;
        float _pad0;
    } params;
    params.invViewProjection = invVP;
    params.lightPosition[0] = m_pointShadowPositions[shadowIndex].x;
    params.lightPosition[1] = m_pointShadowPositions[shadowIndex].y;
    params.lightPosition[2] = m_pointShadowPositions[shadowIndex].z;
    params.lightRange = m_pointShadowRanges[shadowIndex];
    params.nearZ = kPointShadowNearZ;
    params.shadowBias = m_pointShadowBiases[shadowIndex];
    params.overlayAlpha = 0.35f;
    params._pad0 = 0.0f;

    cmdList->SetGraphicsRoot32BitConstants(0, 24, &params, 0);

    ID3D12DescriptorHeap* heaps[] = {ctx.GetSrvHeap().GetHeap()};
    cmdList->SetDescriptorHeaps(1, heaps);
    cmdList->SetGraphicsRootDescriptorTable(1, sceneDepthBuffer.GetSRV().gpu);
    cmdList->SetGraphicsRootDescriptorTable(2, m_pointShadowMaps[shadowIndex].GetSRV().gpu);

    cmdList->DrawInstanced(3, 1, 0, 0);

    // Transition scene depth buffer back: PIXEL_SHADER_RESOURCE -> DEPTH_WRITE
    barrier.Transition.StateBefore =
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    cmdList->ResourceBarrier(1, &barrier);
}

void SceneRenderer::RenderCubemapFaceOverlay(RenderContext& ctx, Camera& camera, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                                             DepthBuffer& sceneDepthBuffer, uint32_t width, uint32_t height,
                                             int shadowIndex) {
    if (shadowIndex < 0 || shadowIndex >= m_numPointShadowLights)
        return;

    auto* cmdList = ctx.GetCommandList().Get();

    // Transition scene depth buffer: DEPTH_WRITE -> PIXEL_SHADER_RESOURCE
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = sceneDepthBuffer.GetResource();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barrier.Transition.StateAfter =
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    cmdList->ResourceBarrier(1, &barrier);

    // Bind RTV only (no depth test needed)
    cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

    D3D12_VIEWPORT viewport = {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
    D3D12_RECT scissor = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    cmdList->RSSetViewports(1, &viewport);
    cmdList->RSSetScissorRects(1, &scissor);

    cmdList->SetGraphicsRootSignature(m_cubemapFaceOverlayRootSig.Get());
    cmdList->SetPipelineState(m_cubemapFaceOverlayPSO.Get());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Root constants: invViewProjection + point light params
    Matrix vp = camera.GetViewMatrix() * camera.GetProjectionMatrix();
    Matrix invVP = vp.Invert();

    struct FaceOverlayParams {
        Matrix invViewProjection;
        float lightPosition[3];
        float lightRange;
        float overlayAlpha;
        float _pad0[3];
    } params;
    params.invViewProjection = invVP;
    params.lightPosition[0] = m_pointShadowPositions[shadowIndex].x;
    params.lightPosition[1] = m_pointShadowPositions[shadowIndex].y;
    params.lightPosition[2] = m_pointShadowPositions[shadowIndex].z;
    params.lightRange = m_pointShadowRanges[shadowIndex];
    params.overlayAlpha = 0.35f;
    params._pad0[0] = 0.0f;
    params._pad0[1] = 0.0f;
    params._pad0[2] = 0.0f;

    cmdList->SetGraphicsRoot32BitConstants(0, 24, &params, 0);

    ID3D12DescriptorHeap* heaps[] = {ctx.GetSrvHeap().GetHeap()};
    cmdList->SetDescriptorHeaps(1, heaps);
    cmdList->SetGraphicsRootDescriptorTable(1, sceneDepthBuffer.GetSRV().gpu);

    cmdList->DrawInstanced(3, 1, 0, 0);

    // Transition scene depth buffer back: PIXEL_SHADER_RESOURCE -> DEPTH_WRITE
    barrier.Transition.StateBefore =
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    cmdList->ResourceBarrier(1, &barrier);
}

void SceneRenderer::RenderDepthHeatmapOverlay(RenderContext& ctx, Camera& camera, D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                                              DepthBuffer& sceneDepthBuffer, uint32_t width, uint32_t height,
                                              int shadowIndex) {
    if (shadowIndex < 0 || shadowIndex >= m_numPointShadowLights)
        return;

    auto* cmdList = ctx.GetCommandList().Get();

    // Transition scene depth buffer: DEPTH_WRITE -> PIXEL_SHADER_RESOURCE
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = sceneDepthBuffer.GetResource();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    barrier.Transition.StateAfter =
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    cmdList->ResourceBarrier(1, &barrier);

    // Bind RTV only (no depth test needed)
    cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

    D3D12_VIEWPORT viewport = {0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
    D3D12_RECT scissor = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    cmdList->RSSetViewports(1, &viewport);
    cmdList->RSSetScissorRects(1, &scissor);

    cmdList->SetGraphicsRootSignature(m_cubemapFaceOverlayRootSig.Get());
    cmdList->SetPipelineState(m_depthHeatmapOverlayPSO.Get());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Root constants: invViewProjection + point light params
    Matrix vp = camera.GetViewMatrix() * camera.GetProjectionMatrix();
    Matrix invVP = vp.Invert();

    struct DepthHeatmapParams {
        Matrix invViewProjection;
        float lightPosition[3];
        float lightRange;
        float overlayAlpha;
        float _pad0[3];
    } params;
    params.invViewProjection = invVP;
    params.lightPosition[0] = m_pointShadowPositions[shadowIndex].x;
    params.lightPosition[1] = m_pointShadowPositions[shadowIndex].y;
    params.lightPosition[2] = m_pointShadowPositions[shadowIndex].z;
    params.lightRange = m_pointShadowRanges[shadowIndex];
    params.overlayAlpha = 0.35f;
    params._pad0[0] = 0.0f;
    params._pad0[1] = 0.0f;
    params._pad0[2] = 0.0f;

    cmdList->SetGraphicsRoot32BitConstants(0, 24, &params, 0);

    ID3D12DescriptorHeap* heaps[] = {ctx.GetSrvHeap().GetHeap()};
    cmdList->SetDescriptorHeaps(1, heaps);
    cmdList->SetGraphicsRootDescriptorTable(1, sceneDepthBuffer.GetSRV().gpu);

    cmdList->DrawInstanced(3, 1, 0, 0);

    // Transition scene depth buffer back: PIXEL_SHADER_RESOURCE -> DEPTH_WRITE
    barrier.Transition.StateBefore =
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    cmdList->ResourceBarrier(1, &barrier);
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

    // Bind point shadow cubemap SRVs (slots 6-11)
    for (int i = 0; i < kMaxPointLights; ++i)
        cmdList->SetGraphicsRootDescriptorTable(6 + i, m_pointShadowMaps[i].GetSRV().gpu);

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

        // Match point light to its shadow cubemap
        frameData.pointLights[i].shadowIndex = -1;
        if (pointLights[i].castShadow) {
            for (int s = 0; s < m_numPointShadowLights; ++s) {
                if (Vector3::DistanceSquared(pointLights[i].position, m_pointShadowPositions[s]) < 0.0001f) {
                    frameData.pointLights[i].shadowIndex = s;
                    frameData.pointLights[i].shadowBias = m_pointShadowBiases[s];
                    break;
                }
            }
        }

        m_cachedPointShadowEntries[i] = {pointLights[i].objectId, frameData.pointLights[i].shadowIndex};
    }
    m_numCachedPointShadowEntries = numPL;
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

    // ViewMode override: force lighting off in Unlit mode
    if (m_viewMode == ViewMode::Unlit) {
        frameData.lightingEnabled = 0;
    }

    // Shadow mapping data (directional)
    if (m_hasShadow) {
        frameData.lightViewProjection = m_cachedLightVP;
        frameData.shadowEnabled = 1;
        frameData.shadowBias = m_cachedShadowBias;
        frameData.shadowMapTexelSize = 1.0f / static_cast<float>(kShadowMapResolution);
    }

    // Point light shadow data
    frameData.numPointShadowLights = m_numPointShadowLights;
    frameData.pointShadowNearZ = kPointShadowNearZ;

    m_perFrameCB[fi].UpdateData(&frameData, sizeof(frameData));
    cmdList->SetGraphicsRootConstantBufferView(0, m_perFrameCB[fi].GetResource()->GetGPUVirtualAddress());

    uint32_t objectIndex = 0;
    D3D12_GPU_VIRTUAL_ADDRESS objCbBase = m_perObjectCB[fi].GetResource()->GetGPUVirtualAddress();
    D3D12_GPU_VIRTUAL_ADDRESS matCbBase = m_perMaterialCB[fi].GetResource()->GetGPUVirtualAddress();

    for (const auto& sceneObj : scene.GetObjects()) {
        if (!sceneObj.HasModel())
            continue;
        if (!sceneObj.enabled)
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

    // Bind dummy SRVs for shadow map slots (required by root signature)
    cmdList->SetGraphicsRootDescriptorTable(5, m_defaultWhiteTex.GetSRVHandle().gpu);
    for (int i = 0; i < kMaxPointLights; ++i)
        cmdList->SetGraphicsRootDescriptorTable(6 + i, m_defaultWhiteTex.GetSRVHandle().gpu);

    // Reuse PerFrame CB (already uploaded by Render())
    uint32_t fi = ctx.GetCurrentFrameIndex();
    cmdList->SetGraphicsRootConstantBufferView(0, m_perFrameCB[fi].GetResource()->GetGPUVirtualAddress());

    uint32_t objectIndex = 0;
    D3D12_GPU_VIRTUAL_ADDRESS objCbBase = m_perObjectCB[fi].GetResource()->GetGPUVirtualAddress();
    D3D12_GPU_VIRTUAL_ADDRESS matCbBase = m_perMaterialCB[fi].GetResource()->GetGPUVirtualAddress();

    for (const auto& sceneObj : scene.GetObjects()) {
        if (!sceneObj.HasModel())
            continue;
        if (!sceneObj.enabled)
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
