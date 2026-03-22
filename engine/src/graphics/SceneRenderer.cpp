#include <showcase/graphics/SceneRenderer.h>
#include <showcase/core/Log.h>
#include <showcase/graphics/PipelineState.h>
#include <showcase/graphics/RenderContext.h>
#include <showcase/graphics/RootSignature.h>
#include <showcase/graphics/CommandList.h>
#include <cfloat>

namespace showcase {

using namespace DirectX;

struct alignas(256) PerFrameData {
    Matrix viewProjection;
    Vector3 cameraPosition;
    float _pad0;
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
    prim.vertexBuffer.InitAsVertexBuffer(device, allocator,
        vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(ModelVertex)),
        sizeof(ModelVertex));
    prim.indexBuffer.InitAsIndexBuffer(device, allocator,
        indices.data(), static_cast<uint32_t>(indices.size() * sizeof(uint32_t)));
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
        {{-0.5f, -0.5f,  0.5f}, { 0, 0, 1}, {0, 1}},
        {{ 0.5f, -0.5f,  0.5f}, { 0, 0, 1}, {1, 1}},
        {{ 0.5f,  0.5f,  0.5f}, { 0, 0, 1}, {1, 0}},
        {{-0.5f,  0.5f,  0.5f}, { 0, 0, 1}, {0, 0}},
        // Back face (z = -0.5)
        {{ 0.5f, -0.5f, -0.5f}, { 0, 0,-1}, {0, 1}},
        {{-0.5f, -0.5f, -0.5f}, { 0, 0,-1}, {1, 1}},
        {{-0.5f,  0.5f, -0.5f}, { 0, 0,-1}, {1, 0}},
        {{ 0.5f,  0.5f, -0.5f}, { 0, 0,-1}, {0, 0}},
        // Top face (y = +0.5)
        {{-0.5f,  0.5f,  0.5f}, { 0, 1, 0}, {0, 1}},
        {{ 0.5f,  0.5f,  0.5f}, { 0, 1, 0}, {1, 1}},
        {{ 0.5f,  0.5f, -0.5f}, { 0, 1, 0}, {1, 0}},
        {{-0.5f,  0.5f, -0.5f}, { 0, 1, 0}, {0, 0}},
        // Bottom face (y = -0.5)
        {{-0.5f, -0.5f, -0.5f}, { 0,-1, 0}, {0, 1}},
        {{ 0.5f, -0.5f, -0.5f}, { 0,-1, 0}, {1, 1}},
        {{ 0.5f, -0.5f,  0.5f}, { 0,-1, 0}, {1, 0}},
        {{-0.5f, -0.5f,  0.5f}, { 0,-1, 0}, {0, 0}},
        // Right face (x = +0.5)
        {{ 0.5f, -0.5f,  0.5f}, { 1, 0, 0}, {0, 1}},
        {{ 0.5f, -0.5f, -0.5f}, { 1, 0, 0}, {1, 1}},
        {{ 0.5f,  0.5f, -0.5f}, { 1, 0, 0}, {1, 0}},
        {{ 0.5f,  0.5f,  0.5f}, { 1, 0, 0}, {0, 0}},
        // Left face (x = -0.5)
        {{-0.5f, -0.5f, -0.5f}, {-1, 0, 0}, {0, 1}},
        {{-0.5f, -0.5f,  0.5f}, {-1, 0, 0}, {1, 1}},
        {{-0.5f,  0.5f,  0.5f}, {-1, 0, 0}, {1, 0}},
        {{-0.5f,  0.5f, -0.5f}, {-1, 0, 0}, {0, 0}},
    };

    uint32_t indices[] = {
         0,  1,  2,   0,  2,  3,   // front
         4,  5,  6,   4,  6,  7,   // back
         8,  9, 10,   8, 10, 11,   // top
        12, 13, 14,  12, 14, 15,   // bottom
        16, 17, 18,  16, 18, 19,   // right
        20, 21, 22,  20, 22, 23,   // left
    };

    MeshPrimitive prim;
    prim.vertexBuffer.InitAsVertexBuffer(device, allocator,
        vertices, sizeof(vertices), sizeof(ModelVertex));
    prim.indexBuffer.InitAsIndexBuffer(device, allocator,
        indices, sizeof(indices));
    prim.indexCount = _countof(indices);
    prim.localAABB = BoundingBox(XMFLOAT3(0, 0, 0), XMFLOAT3(0.5f, 0.5f, 0.5f));

    Mesh mesh;
    mesh.name = "Cube";
    mesh.primitives.push_back(std::move(prim));

    outModel.meshes.push_back(std::move(mesh));
    outModel.localAABB = BoundingBox(XMFLOAT3(0, 0, 0), XMFLOAT3(0.5f, 0.5f, 0.5f));
}

// ── Ray picking ──────────────────────────────────────────────────────

int SceneRenderer::PickObject(int mouseX, int mouseY, const Camera& camera, const Scene& scene,
                              float vpMinX, float vpMinY, float vpMaxX, float vpMaxY) const {
    float vpWidth = vpMaxX - vpMinX;
    float vpHeight = vpMaxY - vpMinY;
    if (vpWidth <= 0 || vpHeight <= 0) return -1;

    float localX = static_cast<float>(mouseX) - vpMinX;
    float localY = static_cast<float>(mouseY) - vpMinY;

    float ndcX = (localX / vpWidth) * 2.0f - 1.0f;
    float ndcY = 1.0f - (localY / vpHeight) * 2.0f;

    Matrix vp = camera.GetViewProjectionMatrix();
    Matrix invVP = vp.Invert();

    XMVECTOR nearPt = XMVector3TransformCoord(
        XMVectorSet(ndcX, ndcY, 0.0f, 1.0f), invVP);
    XMVECTOR farPt = XMVector3TransformCoord(
        XMVectorSet(ndcX, ndcY, 1.0f, 1.0f), invVP);

    XMVECTOR rayOrigin = nearPt;
    XMVECTOR rayDir = XMVector3Normalize(XMVectorSubtract(farPt, nearPt));

    int closestId = -1;
    float closestDist = FLT_MAX;

    for (const auto& obj : scene.GetObjects()) {
        if (!obj.model) continue;

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
        .AddCBV(0)                                                    // slot 0: PerFrame
        .AddCBV(1)                                                    // slot 1: PerObject
        .AddCBV(2)                                                    // slot 2: PerMaterial
        .AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0)   // slot 3: baseColorTex t0
        .AddStaticSampler(0)                                          // s0: linear wrap
        .SetFlags(D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)
        .Build(device);

    // Load shaders
    auto vs = ctx.GetShaderManager().LoadShader("shaders/mesh_vs_vs.cso");
    auto ps = ctx.GetShaderManager().LoadShader("shaders/mesh_ps_ps.cso");

    // PSO
    GraphicsPipelineDesc psoDesc;
    psoDesc.rootSignature = m_rootSignature.Get();
    psoDesc.vertexShader = vs;
    psoDesc.pixelShader = ps;
    psoDesc.inputLayout = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24,
         D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
    psoDesc.rtvFormat = ctx.GetSwapChain().GetFormat();
    psoDesc.dsvFormat = DXGI_FORMAT_D32_FLOAT;

    m_pipelineState = PipelineState::CreateGraphicsPSO(device, psoDesc);

    // Constant buffers (upload heap, 256-byte aligned)
    m_perFrameCB.InitAsUploadBuffer(device, allocator, sizeof(PerFrameData));
    m_perObjectCB.InitAsUploadBuffer(device, allocator, sizeof(PerObjectData) * kMaxObjects);
    m_perMaterialCB.InitAsUploadBuffer(device, allocator, sizeof(PerMaterialData) * kMaxObjects);

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
        m_defaultWhiteTex.InitFromMemory(device, allocator, texCmdList.Get(),
            ctx.GetSrvHeap(), whitePixel, 1, 1, 4);

        texCmdList.Close();
        ctx.GetDirectQueue().ExecuteCommandList(texCmdList.Get());
        ctx.GetDirectQueue().Flush();
        m_defaultWhiteTex.ReleaseUploadResources();
        texCmdList.Shutdown();
    }

    SE_LOG_INFO("SceneRenderer initialized");
}

void SceneRenderer::Shutdown() {
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
    m_perFrameCB.UpdateData(&frameData, sizeof(frameData));
    cmdList->SetGraphicsRootConstantBufferView(0, m_perFrameCB.GetResource()->GetGPUVirtualAddress());

    uint32_t objectIndex = 0;
    auto objCbBase = m_perObjectCB.GetResource()->GetGPUVirtualAddress();
    auto matCbBase = m_perMaterialCB.GetResource()->GetGPUVirtualAddress();

    for (const auto& sceneObj : scene.GetObjects()) {
        if (!sceneObj.model) continue;

        bool isSelected = (static_cast<int>(sceneObj.id) == selectedObjectId);

        for (const auto& mesh : sceneObj.model->meshes) {
            for (const auto& prim : mesh.primitives) {
                if (prim.indexCount == 0 || objectIndex >= kMaxObjects) continue;

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
                        cmdList->SetGraphicsRootDescriptorTable(3,
                            sceneObj.model->textures[mat.baseColorTextureIndex].GetSRVHandle().gpu);
                    } else {
                        cmdList->SetGraphicsRootDescriptorTable(3, m_defaultWhiteTex.GetSRVHandle().gpu);
                    }
                } else {
                    cmdList->SetGraphicsRootDescriptorTable(3, m_defaultWhiteTex.GetSRVHandle().gpu);
                }

                m_perMaterialCB.UpdateDataAtOffset(&matData, sizeof(matData), objectIndex * sizeof(PerMaterialData));
                cmdList->SetGraphicsRootConstantBufferView(2, matCbBase + objectIndex * sizeof(PerMaterialData));

                auto vbView = prim.vertexBuffer.GetVertexBufferView();
                auto ibView = prim.indexBuffer.GetIndexBufferView();
                cmdList->IASetVertexBuffers(0, 1, &vbView);
                cmdList->IASetIndexBuffer(&ibView);
                cmdList->DrawIndexedInstanced(prim.indexCount, 1, 0, 0, 0);
                objectIndex++;
            }
        }
    }
}

} // namespace showcase
