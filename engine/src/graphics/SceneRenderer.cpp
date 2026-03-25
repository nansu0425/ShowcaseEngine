#include <showcase/graphics/SceneRenderer.h>

#include <showcase/core/Log.h>
#include <showcase/core/Profiler.h>
#include <showcase/graphics/CommandList.h>
#include <showcase/graphics/PipelineState.h>
#include <showcase/graphics/RenderContext.h>
#include <showcase/graphics/RootSignature.h>

#include <cfloat>

namespace showcase {

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

static_assert(sizeof(PerFrameData) <= 256);
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
    if (!prim.vertexBuffer.InitAsVertexBuffer(device, allocator, vertices, sizeof(vertices), sizeof(ModelVertex))) {
        SE_LOG_ERROR("Failed to create cube vertex buffer");
        return;
    }
    if (!prim.indexBuffer.InitAsIndexBuffer(device, allocator, indices, sizeof(indices))) {
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
    if (!prim.vertexBuffer.InitAsVertexBuffer(device, allocator, vertices, sizeof(vertices), sizeof(ModelVertex))) {
        SE_LOG_ERROR("Failed to create plane vertex buffer");
        return;
    }
    if (!prim.indexBuffer.InitAsIndexBuffer(device, allocator, indices, sizeof(indices))) {
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
    if (!prim.vertexBuffer.InitAsVertexBuffer(device, allocator, vertices.data(),
                                              static_cast<uint32_t>(vertices.size() * sizeof(ModelVertex)),
                                              sizeof(ModelVertex))) {
        SE_LOG_ERROR("Failed to create sphere vertex buffer");
        return;
    }
    if (!prim.indexBuffer.InitAsIndexBuffer(device, allocator, indices.data(),
                                            static_cast<uint32_t>(indices.size() * sizeof(uint32_t)))) {
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

    Vector3 nearPt = Vector3::Transform(Vector3(ndcX, ndcY, 0.0f), invVP);
    Vector3 farPt = Vector3::Transform(Vector3(ndcX, ndcY, 1.0f), invVP);

    Vector3 rayOrigin = nearPt;
    Vector3 rayDir = farPt - nearPt;
    rayDir.Normalize();

    int closestId = -1;
    float closestDist = FLT_MAX;

    for (const auto& obj : scene.GetObjects()) {
        if (!obj.HasModel())
            continue;

        // Transform ray into object local space for accurate rotated picking
        Matrix invWorld = obj.worldTransform.Invert();
        Vector3 localOrigin = Vector3::Transform(rayOrigin, invWorld);
        Vector3 localDir = Vector3::TransformNormal(rayDir, invWorld);
        localDir.Normalize();

        float dist = 0.0f;
        if (obj.modelComp->model->localAABB.Intersects(localOrigin, localDir, dist)) {
            // Convert local-space hit point back to world-space distance
            Vector3 localHit = localOrigin + localDir * dist;
            Vector3 worldHit = Vector3::Transform(localHit, obj.worldTransform);
            float worldDist = (worldHit - rayOrigin).Length();

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
    SE_ZONE_SCOPED;
    auto* device = ctx.GetDevice().GetDevice();
    auto* allocator = ctx.GetDevice().GetAllocator();

    // Root signature: CBV b0 (PerFrame), CBV b1 (PerObject), CBV b2 (PerMaterial),
    //                 DescriptorTable(SRV t0), DescriptorTable(SRV t1), StaticSampler s0
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
    SE_ZONE_SCOPED_C(profile::kColorRendering);
    SE_PLOT("Scene Objects", static_cast<int64_t>(scene.GetObjects().size()));
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
    D3D12_GPU_VIRTUAL_ADDRESS objCbBase = m_perObjectCB.GetResource()->GetGPUVirtualAddress();
    D3D12_GPU_VIRTUAL_ADDRESS matCbBase = m_perMaterialCB.GetResource()->GetGPUVirtualAddress();

    for (const auto& sceneObj : scene.GetObjects()) {
        if (!sceneObj.HasModel())
            continue;

        bool isSelected = (static_cast<int>(sceneObj.id) == selectedObjectId);

        for (const auto& mesh : sceneObj.modelComp->model->meshes) {
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

                if (prim.material) {
                    matData.baseColorFactor = prim.material->baseColorFactor;

                    if (prim.material->baseColorTexture) {
                        matData.hasTexture = 1;
                        cmdList->SetGraphicsRootDescriptorTable(3, prim.material->baseColorTexture->GetSRVHandle().gpu);
                    } else {
                        cmdList->SetGraphicsRootDescriptorTable(3, m_defaultWhiteTex.GetSRVHandle().gpu);
                    }
                } else {
                    cmdList->SetGraphicsRootDescriptorTable(3, m_defaultWhiteTex.GetSRVHandle().gpu);
                }

                // Per-object color override takes priority over material color
                if (sceneObj.modelComp->baseColor.has_value()) {
                    matData.baseColorFactor = *sceneObj.modelComp->baseColor;
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

    SE_PLOT("Draw Calls", static_cast<int64_t>(objectIndex));
}

} // namespace showcase
