#include <showcase/graphics/SceneRenderer.h>

#include <showcase/core/Log.h>
#include <showcase/graphics/CommandList.h>
#include <showcase/graphics/PipelineState.h>
#include <showcase/graphics/RenderContext.h>
#include <showcase/graphics/RootSignature.h>

#include <cfloat>
#include <cmath>

namespace showcase {

using namespace DirectX;

struct alignas(256) PerFrameData {
    Matrix viewProjection;
    Vector3 cameraPosition;
    float _pad0;
    float gridFadeStart;
    float gridFadeEnd;
    float gridOpacity;
    float _pad1;
};

struct alignas(256) PerObjectData {
    Matrix world;
};

struct alignas(256) PerMaterialData {
    Vector4 baseColorFactor;
    int hasTexture;
    float selectionTint;
    float _pad[2];
};

static_assert(sizeof(PerFrameData) <= 256);
static_assert(sizeof(PerObjectData) <= 256);
static_assert(sizeof(PerMaterialData) <= 256);

struct GridVertex {
    Vector3 position;
    Vector4 color;
};

// ── Procedural geometry ──────────────────────────────────────────────

void SceneRenderer::CreateGridModel(RenderContext& ctx, Model& outModel) {
    auto* device = ctx.GetDevice().GetDevice();
    auto* allocator = ctx.GetDevice().GetAllocator();
    const int gridSize = 20;
    const float cellSize = 2.0f;
    const float halfExtent = gridSize * cellSize * 0.5f;

    std::vector<ModelVertex> vertices;
    std::vector<uint32_t> indices;

    for (int z = 0; z <= gridSize; ++z) {
        for (int x = 0; x <= gridSize; ++x) {
            ModelVertex v;
            v.position = Vector3(x * cellSize - halfExtent, 0.0f, z * cellSize - halfExtent);
            v.normal = Vector3(0.0f, 1.0f, 0.0f);
            v.texCoord = Vector2(static_cast<float>(x) / gridSize, static_cast<float>(z) / gridSize);
            vertices.push_back(v);
        }
    }

    for (int z = 0; z < gridSize; ++z) {
        for (int x = 0; x < gridSize; ++x) {
            uint32_t topLeft = z * (gridSize + 1) + x;
            uint32_t topRight = topLeft + 1;
            uint32_t bottomLeft = (z + 1) * (gridSize + 1) + x;
            uint32_t bottomRight = bottomLeft + 1;

            indices.push_back(topLeft);
            indices.push_back(bottomLeft);
            indices.push_back(topRight);

            indices.push_back(topRight);
            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
        }
    }

    MeshPrimitive prim;
    if (!prim.vertexBuffer.InitAsVertexBuffer(device, allocator, vertices.data(),
                                              static_cast<uint32_t>(vertices.size() * sizeof(ModelVertex)),
                                              sizeof(ModelVertex))) {
        SE_LOG_ERROR("Failed to create grid vertex buffer");
        return;
    }
    if (!prim.indexBuffer.InitAsIndexBuffer(device, allocator, indices.data(),
                                            static_cast<uint32_t>(indices.size() * sizeof(uint32_t)))) {
        SE_LOG_ERROR("Failed to create grid index buffer");
        return;
    }
    prim.indexCount = static_cast<uint32_t>(indices.size());
    prim.localAABB = BoundingBox(XMFLOAT3(0, 0, 0), XMFLOAT3(halfExtent, 0.01f, halfExtent));

    Mesh mesh;
    mesh.name = "Grid";
    mesh.primitives.push_back(std::move(prim));

    outModel.meshes.push_back(std::move(mesh));
    outModel.localAABB = BoundingBox(XMFLOAT3(0, 0, 0), XMFLOAT3(halfExtent, 0.01f, halfExtent));
}

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
    if (!prim.vertexBuffer.InitAsVertexBuffer(device, allocator, vertices, sizeof(vertices), sizeof(ModelVertex))) {
        SE_LOG_ERROR("Failed to create cube vertex buffer");
        return;
    }
    if (!prim.indexBuffer.InitAsIndexBuffer(device, allocator, indices, sizeof(indices))) {
        SE_LOG_ERROR("Failed to create cube index buffer");
        return;
    }
    prim.indexCount = _countof(indices);
    prim.localAABB = BoundingBox(XMFLOAT3(0, 0, 0), XMFLOAT3(0.5f, 0.5f, 0.5f));

    Mesh mesh;
    mesh.name = "Cube";
    mesh.primitives.push_back(std::move(prim));

    outModel.meshes.push_back(std::move(mesh));
    outModel.localAABB = BoundingBox(XMFLOAT3(0, 0, 0), XMFLOAT3(0.5f, 0.5f, 0.5f));
}

// ── Ray picking ──────────────────────────────────────────────────────

int SceneRenderer::PickObject(int mouseX, int mouseY, const Camera& camera, const Scene& scene, float vpMinX,
                              float vpMinY, float vpMaxX, float vpMaxY) const {
    float vpWidth = vpMaxX - vpMinX;
    float vpHeight = vpMaxY - vpMinY;
    if (vpWidth <= 0 || vpHeight <= 0)
        return -1;

    float localX = static_cast<float>(mouseX) - vpMinX;
    float localY = static_cast<float>(mouseY) - vpMinY;

    float ndcX = (localX / vpWidth) * 2.0f - 1.0f;
    float ndcY = 1.0f - (localY / vpHeight) * 2.0f;

    Matrix vp = camera.GetViewProjectionMatrix();
    Matrix invVP = vp.Invert();

    XMVECTOR nearPt = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 0.0f, 1.0f), invVP);
    XMVECTOR farPt = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 1.0f, 1.0f), invVP);

    XMVECTOR rayOrigin = nearPt;
    XMVECTOR rayDir = XMVector3Normalize(XMVectorSubtract(farPt, nearPt));

    int closestId = -1;
    float closestDist = FLT_MAX;

    for (const auto& obj : scene.GetObjects()) {
        if (!obj.model)
            continue;

        // Transform ray into object local space for accurate rotated picking
        Matrix invWorld = obj.worldTransform.Invert();
        XMVECTOR localOrigin = XMVector3TransformCoord(rayOrigin, invWorld);
        XMVECTOR localDir = XMVector3Normalize(XMVector3TransformNormal(rayDir, invWorld));

        float dist = 0.0f;
        if (obj.model->localAABB.Intersects(localOrigin, localDir, dist)) {
            // Convert local-space hit point back to world-space distance
            XMVECTOR localHit = XMVectorAdd(localOrigin, XMVectorScale(localDir, dist));
            XMVECTOR worldHit = XMVector3TransformCoord(localHit, obj.worldTransform);
            float worldDist = XMVectorGetX(XMVector3Length(XMVectorSubtract(worldHit, rayOrigin)));

            if (worldDist < closestDist) {
                closestDist = worldDist;
                closestId = static_cast<int>(obj.id);
            }
        }
    }

    return closestId;
}

// ── Init / Shutdown ──────────────────────────────────────────────────

void SceneRenderer::Init(RenderContext& ctx) {
    auto* device = ctx.GetDevice().GetDevice();
    auto* allocator = ctx.GetDevice().GetAllocator();

    // Root signature: CBV b0 (PerFrame), CBV b1 (PerObject), CBV b2 (PerMaterial),
    //                 DescriptorTable(SRV t0), StaticSampler s0
    m_rootSignature = RootSignatureBuilder()
                          .AddCBV(0)                                                 // slot 0: PerFrame
                          .AddCBV(1)                                                 // slot 1: PerObject
                          .AddCBV(2)                                                 // slot 2: PerMaterial
                          .AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0) // slot 3: baseColorTex t0
                          .AddStaticSampler(0)                                       // s0: linear wrap
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

    // Constant buffers (upload heap, 256-byte aligned)
    if (!m_perFrameCB.InitAsUploadBuffer(device, allocator, sizeof(PerFrameData)) ||
        !m_perObjectCB.InitAsUploadBuffer(device, allocator, sizeof(PerObjectData) * kMaxObjects) ||
        !m_perMaterialCB.InitAsUploadBuffer(device, allocator, sizeof(PerMaterialData) * kMaxObjects)) {
        SE_LOG_ERROR("Failed to create constant buffers");
        return;
    }

    // Store SRV heap pointer for cleanup
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
        if (!m_defaultWhiteTex.InitFromMemory(device, allocator, texCmdList.Get(), ctx.GetSrvHeap(), whitePixel, 1, 1,
                                              4)) {
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

    // Grid/axis shared shader + blend/depth settings
    D3D12_SHADER_BYTECODE gridVS = ctx.GetShaderManager().LoadShader("shaders/grid_vs_vs.cso");
    D3D12_SHADER_BYTECODE gridPS = ctx.GetShaderManager().LoadShader("shaders/grid_ps_ps.cso");

    D3D12_BLEND_DESC gridBlend = DefaultBlendDesc();
    gridBlend.RenderTarget[0].BlendEnable = TRUE;
    gridBlend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    gridBlend.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    gridBlend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;

    D3D12_DEPTH_STENCIL_DESC gridDS = DefaultDepthStencilDesc();
    gridDS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

    std::vector<D3D12_INPUT_ELEMENT_DESC> gridInputLayout = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    // Grid quad PSO (TRIANGLE topology, procedural grid in PS)
    GraphicsPipelineDesc gridPsoDesc;
    gridPsoDesc.rootSignature = m_rootSignature.Get();
    gridPsoDesc.vertexShader = gridVS;
    gridPsoDesc.pixelShader = gridPS;
    gridPsoDesc.inputLayout = gridInputLayout;
    gridPsoDesc.primitiveTopology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    gridPsoDesc.rtvFormat = ctx.GetSwapChain().GetFormat();
    gridPsoDesc.dsvFormat = DXGI_FORMAT_D32_FLOAT;
    gridPsoDesc.blendState = gridBlend;
    gridPsoDesc.depthStencilState = gridDS;
    gridPsoDesc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    m_gridPSO = PipelineState::CreateGraphicsPSO(device, gridPsoDesc);

    // Axis line PSO (LINE topology)
    GraphicsPipelineDesc axisPsoDesc;
    axisPsoDesc.rootSignature = m_rootSignature.Get();
    axisPsoDesc.vertexShader = gridVS;
    axisPsoDesc.pixelShader = gridPS;
    axisPsoDesc.inputLayout = gridInputLayout;
    axisPsoDesc.primitiveTopology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    axisPsoDesc.rtvFormat = ctx.GetSwapChain().GetFormat();
    axisPsoDesc.dsvFormat = DXGI_FORMAT_D32_FLOAT;
    axisPsoDesc.blendState = gridBlend;
    axisPsoDesc.depthStencilState = gridDS;
    axisPsoDesc.rasterizerState.AntialiasedLineEnable = TRUE;
    axisPsoDesc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    m_axisLinePSO = PipelineState::CreateGraphicsPSO(device, axisPsoDesc);

    CreateGridQuad(device, allocator, 200.0f);
    CreateAxisLines(device, allocator);

    // Grid offset CB: 2 slots (grid camera offset + axis identity)
    if (!m_gridOffsetCB.InitAsUploadBuffer(device, allocator, 256 * 2)) {
        SE_LOG_ERROR("Failed to create grid offset constant buffer");
        return;
    }

    SE_LOG_INFO("SceneRenderer initialized");
}

void SceneRenderer::CreateGridQuad(ID3D12Device* device, D3D12MA::Allocator* allocator, float halfExtent) {
    const Vector4 quadColor(0.0f, 0.0f, 0.0f, 0.0f); // alpha=0 signals procedural mode in PS

    GridVertex vertices[] = {
        {Vector3(-halfExtent, 0.0f, -halfExtent), quadColor}, {Vector3(-halfExtent, 0.0f, halfExtent), quadColor},
        {Vector3(halfExtent, 0.0f, halfExtent), quadColor},   {Vector3(-halfExtent, 0.0f, -halfExtent), quadColor},
        {Vector3(halfExtent, 0.0f, halfExtent), quadColor},   {Vector3(halfExtent, 0.0f, -halfExtent), quadColor},
    };

    m_gridVertexCount = 6;
    if (!m_gridVertexBuffer.InitAsVertexBuffer(device, allocator, vertices, sizeof(vertices), sizeof(GridVertex))) {
        SE_LOG_ERROR("Failed to create grid quad vertex buffer");
    }
}

void SceneRenderer::CreateAxisLines(ID3D12Device* device, D3D12MA::Allocator* allocator) {
    const float len = 10000.0f;
    const Vector4 xColor(0.7f, 0.2f, 0.2f, 0.8f);
    const Vector4 yColor(0.2f, 0.7f, 0.2f, 0.8f);
    const Vector4 zColor(0.2f, 0.2f, 0.7f, 0.8f);

    GridVertex vertices[] = {
        {Vector3(-len, 0.0f, 0.0f), xColor}, {Vector3(len, 0.0f, 0.0f), xColor},  {Vector3(0.0f, -len, 0.0f), yColor},
        {Vector3(0.0f, len, 0.0f), yColor},  {Vector3(0.0f, 0.0f, -len), zColor}, {Vector3(0.0f, 0.0f, len), zColor},
    };
    m_axisVertexCount = 6;
    if (!m_axisVertexBuffer.InitAsVertexBuffer(device, allocator, vertices, sizeof(vertices), sizeof(GridVertex))) {
        SE_LOG_ERROR("Failed to create axis line vertex buffer");
    }
}

void SceneRenderer::RenderGrid(ID3D12GraphicsCommandList* cmdList, const Camera& camera) {
    if (m_gridVertexCount == 0)
        return;

    struct alignas(256) GridWorldData {
        Matrix world;
    };
    static_assert(sizeof(GridWorldData) == 256);

    // Slot 0: grid offset — snap camera XZ to 1m grid spacing
    Vector3 camPos = camera.GetPosition();
    GridWorldData gridData{Matrix::CreateTranslation(std::floor(camPos.x), 0.0f, std::floor(camPos.z))};
    m_gridOffsetCB.UpdateDataAtOffset(&gridData, sizeof(gridData), 0);

    // Slot 1: identity — axis lines at world origin
    GridWorldData axisData{Matrix()};
    m_gridOffsetCB.UpdateDataAtOffset(&axisData, sizeof(axisData), sizeof(GridWorldData));

    auto cbBase = m_gridOffsetCB.GetResource()->GetGPUVirtualAddress();

    // Draw procedural grid quad (TRIANGLE topology)
    cmdList->SetPipelineState(m_gridPSO.Get());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->SetGraphicsRootConstantBufferView(1, cbBase);
    D3D12_VERTEX_BUFFER_VIEW gridVB = m_gridVertexBuffer.GetVertexBufferView();
    cmdList->IASetVertexBuffers(0, 1, &gridVB);
    cmdList->DrawInstanced(m_gridVertexCount, 1, 0, 0);

    // Draw axis lines (LINE topology, fixed at world origin)
    if (m_axisVertexCount > 0) {
        cmdList->SetPipelineState(m_axisLinePSO.Get());
        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
        cmdList->SetGraphicsRootConstantBufferView(1, cbBase + sizeof(GridWorldData));
        D3D12_VERTEX_BUFFER_VIEW axisVB = m_axisVertexBuffer.GetVertexBufferView();
        cmdList->IASetVertexBuffers(0, 1, &axisVB);
        cmdList->DrawInstanced(m_axisVertexCount, 1, 0, 0);
    }
}

void SceneRenderer::Shutdown() {
    m_axisVertexBuffer.Shutdown();
    m_gridOffsetCB.Shutdown();
    m_gridVertexBuffer.Shutdown();
    m_gridPSO.Reset();
    m_axisLinePSO.Reset();
    if (m_srvHeap) {
        m_defaultWhiteTex.Shutdown(*m_srvHeap);
    }
    m_perFrameCB.Shutdown();
    m_perObjectCB.Shutdown();
    m_perMaterialCB.Shutdown();
    m_pipelineState.Reset();
    m_rootSignature.Reset();
}

void SceneRenderer::OnResize(uint32_t width, uint32_t height) {
    m_aspectRatio = height > 0 ? static_cast<float>(width) / height : 1.0f;
}

// ── Render ───────────────────────────────────────────────────────────

void SceneRenderer::Render(RenderContext& ctx, Camera& camera, Scene& scene, int selectedObjectId) {
    auto* cmdList = ctx.GetCommandList().Get();

    cmdList->SetGraphicsRootSignature(m_rootSignature.Get());
    cmdList->SetPipelineState(m_pipelineState.Get());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Update PerFrame CB
    PerFrameData frameData;
    frameData.viewProjection = camera.GetViewProjectionMatrix();
    frameData.cameraPosition = camera.GetPosition();
    frameData.gridFadeStart = m_gridSettings.fadeStart;
    frameData.gridFadeEnd = m_gridSettings.fadeEnd;
    frameData.gridOpacity = m_gridSettings.opacity;
    m_perFrameCB.UpdateData(&frameData, sizeof(frameData));
    cmdList->SetGraphicsRootConstantBufferView(0, m_perFrameCB.GetResource()->GetGPUVirtualAddress());

    // Draw grid lines (depth write OFF — objects will paint over)
    if (m_gridSettings.visible) {
        RenderGrid(cmdList, camera);
    }

    // Restore mesh pipeline state
    cmdList->SetPipelineState(m_pipelineState.Get());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    uint32_t objectIndex = 0;
    D3D12_GPU_VIRTUAL_ADDRESS objCbBase = m_perObjectCB.GetResource()->GetGPUVirtualAddress();
    D3D12_GPU_VIRTUAL_ADDRESS matCbBase = m_perMaterialCB.GetResource()->GetGPUVirtualAddress();

    for (const auto& sceneObj : scene.GetObjects()) {
        if (!sceneObj.model)
            continue;

        bool isSelected = (static_cast<int>(sceneObj.id) == selectedObjectId);

        for (const auto& mesh : sceneObj.model->meshes) {
            for (const auto& prim : mesh.primitives) {
                if (prim.indexCount == 0 || objectIndex >= kMaxObjects)
                    continue;

                // Per-object transform
                PerObjectData objData;
                objData.world = sceneObj.worldTransform;
                m_perObjectCB.UpdateDataAtOffset(&objData, sizeof(objData), objectIndex * sizeof(PerObjectData));
                cmdList->SetGraphicsRootConstantBufferView(1, objCbBase + objectIndex * sizeof(PerObjectData));

                // Per-material
                PerMaterialData matData;
                matData.baseColorFactor = Vector4(0.7f, 0.7f, 0.7f, 1.0f);
                matData.hasTexture = 0;
                matData.selectionTint = isSelected ? 1.0f : 0.0f;

                if (prim.materialIndex >= 0 &&
                    prim.materialIndex < static_cast<int>(sceneObj.model->materials.size())) {
                    const auto& mat = sceneObj.model->materials[prim.materialIndex];
                    matData.baseColorFactor = mat.baseColorFactor;

                    if (mat.baseColorTextureIndex >= 0 &&
                        mat.baseColorTextureIndex < static_cast<int>(sceneObj.model->textures.size())) {
                        matData.hasTexture = 1;
                        cmdList->SetGraphicsRootDescriptorTable(
                            3, sceneObj.model->textures[mat.baseColorTextureIndex].GetSRVHandle().gpu);
                    } else {
                        cmdList->SetGraphicsRootDescriptorTable(3, m_defaultWhiteTex.GetSRVHandle().gpu);
                    }
                } else {
                    cmdList->SetGraphicsRootDescriptorTable(3, m_defaultWhiteTex.GetSRVHandle().gpu);
                }

                m_perMaterialCB.UpdateDataAtOffset(&matData, sizeof(matData), objectIndex * sizeof(PerMaterialData));
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
}

} // namespace showcase
