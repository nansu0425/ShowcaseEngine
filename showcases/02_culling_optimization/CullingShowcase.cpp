#include "CullingShowcase.h"
#include <showcase/core/Log.h>
#include <showcase/core/Input.h>
#include <showcase/demo/ShowcaseRegistry.h>
#include <showcase/graphics/PipelineState.h>
#include <showcase/graphics/RenderContext.h>
#include <showcase/graphics/RootSignature.h>
#include <showcase/graphics/Model.h>
#include <showcase/graphics/CommandList.h>
#include <imgui.h>
#include <filesystem>

namespace showcase {

using namespace DirectX::SimpleMath;

REGISTER_SHOWCASE(CullingShowcase)

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
    int _pad[3];
};

// ── Procedural geometry ──────────────────────────────────────────────

void CullingShowcase::CreateGridMesh(ID3D12Device* device, D3D12MA::Allocator* allocator) {
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

    m_gridIndexCount = static_cast<uint32_t>(indices.size());

    m_gridVB.InitAsVertexBuffer(device, allocator,
        vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(ModelVertex)),
        sizeof(ModelVertex));
    m_gridIB.InitAsIndexBuffer(device, allocator,
        indices.data(), static_cast<uint32_t>(indices.size() * sizeof(uint32_t)));
}

void CullingShowcase::CreateCubeMesh(ID3D12Device* device, D3D12MA::Allocator* allocator) {
    // Unit cube centered at origin, with normals
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

    m_cubeIndexCount = _countof(indices);

    m_cubeVB.InitAsVertexBuffer(device, allocator,
        vertices, sizeof(vertices), sizeof(ModelVertex));
    m_cubeIB.InitAsIndexBuffer(device, allocator,
        indices, sizeof(indices));
}

// ── Lifecycle ────────────────────────────────────────────────────────

void CullingShowcase::OnLoad(RenderContext& ctx) {
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

    // Store SRV heap pointer for cleanup in OnUnload
    m_srvHeap = &ctx.GetSrvHeap();

    // Create 1x1 white default texture for untextured geometry
    {
        CommandList texCmdList;
        texCmdList.Init(device, D3D12_COMMAND_LIST_TYPE_DIRECT);
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

    // Geometry
    CreateGridMesh(device, allocator);
    CreateCubeMesh(device, allocator);

    // Place some cubes in the scene
    m_cubes.clear();
    m_cubes.push_back({Matrix::CreateScale(2.0f) * Matrix::CreateTranslation(0.0f, 1.0f, 0.0f)});
    m_cubes.push_back({Matrix::CreateScale(1.5f) * Matrix::CreateTranslation(5.0f, 0.75f, 3.0f)});
    m_cubes.push_back({Matrix::CreateScale(3.0f, 4.0f, 3.0f) * Matrix::CreateTranslation(-6.0f, 2.0f, -4.0f)});
    m_cubes.push_back({Matrix::CreateScale(1.0f) * Matrix::CreateTranslation(8.0f, 0.5f, -7.0f)});
    m_cubes.push_back({Matrix::CreateScale(2.0f, 1.0f, 2.0f) * Matrix::CreateTranslation(-3.0f, 0.5f, 8.0f)});

    // Try loading a glTF model from assets/models/
    {
        // Resolve path relative to executable directory
        char exePath[MAX_PATH];
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        std::filesystem::path modelDir = std::filesystem::path(exePath).parent_path() / "assets" / "models";

        // Look for the first .gltf or .glb file in the directory
        if (std::filesystem::exists(modelDir)) {
            for (const auto& entry : std::filesystem::directory_iterator(modelDir)) {
                auto ext = entry.path().extension().string();
                if (ext == ".gltf" || ext == ".glb") {
                    m_modelLoaded = ModelLoader::LoadGLTF(
                        entry.path().string(), device, allocator,
                        ctx.GetDirectQueue(), ctx.GetSrvHeap(), m_testModel);
                    if (m_modelLoaded) {
                        SE_LOG_INFO("Loaded test model: {}", entry.path().filename().string());
                    }
                    break;
                }
            }
        }

        if (!m_modelLoaded) {
            SE_LOG_INFO("No glTF model found in assets/models/ — rendering procedural geometry only");
        }
    }

    // Camera initial position
    m_camera.SetPosition(Vector3(0.0f, 5.0f, -15.0f));
    m_camera.SetLookAt(Vector3(0.0f, 0.0f, 0.0f));
    m_camera.SetPerspective(DirectX::XM_PIDIV4, m_aspectRatio, 0.1f, 1000.0f);
    m_camera.UpdateViewMatrix();

    SE_LOG_INFO("CullingShowcase loaded");
}

void CullingShowcase::OnUnload() {
    if (m_modelLoaded && m_srvHeap) {
        m_testModel.Shutdown(*m_srvHeap);
        m_modelLoaded = false;
    }
    if (m_srvHeap) {
        m_defaultWhiteTex.Shutdown(*m_srvHeap);
    }
    m_gridVB.Shutdown();
    m_gridIB.Shutdown();
    m_cubeVB.Shutdown();
    m_cubeIB.Shutdown();
    m_perFrameCB.Shutdown();
    m_perObjectCB.Shutdown();
    m_perMaterialCB.Shutdown();
    m_pipelineState.Reset();
    m_rootSignature.Reset();
}

void CullingShowcase::OnResize(uint32_t width, uint32_t height) {
    m_aspectRatio = height > 0 ? static_cast<float>(width) / height : 1.0f;
    m_camera.SetPerspective(m_camera.GetFovY(), m_aspectRatio, m_camera.GetNearZ(), m_camera.GetFarZ());
}

void CullingShowcase::OnUpdate(float deltaTime, const Input& input) {
    m_cameraController.Update(m_camera, input, deltaTime);
}

void CullingShowcase::OnRender(RenderContext& ctx) {
    auto* cmdList = ctx.GetCommandList().Get();

    cmdList->SetGraphicsRootSignature(m_rootSignature.Get());
    cmdList->SetPipelineState(m_pipelineState.Get());
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Update PerFrame CB
    PerFrameData frameData;
    frameData.viewProjection = m_camera.GetViewProjectionMatrix();
    frameData.cameraPosition = m_camera.GetPosition();
    m_perFrameCB.UpdateData(&frameData, sizeof(frameData));
    cmdList->SetGraphicsRootConstantBufferView(0, m_perFrameCB.GetResource()->GetGPUVirtualAddress());

    uint32_t objectIndex = 0;
    auto objCbBase = m_perObjectCB.GetResource()->GetGPUVirtualAddress();
    auto matCbBase = m_perMaterialCB.GetResource()->GetGPUVirtualAddress();

    // Helper: bind material for untextured procedural geometry
    auto bindDefaultMaterial = [&](const Vector4& color) {
        PerMaterialData matData;
        matData.baseColorFactor = color;
        matData.hasTexture = 0;
        m_perMaterialCB.UpdateDataAtOffset(&matData, sizeof(matData), objectIndex * sizeof(PerMaterialData));
        cmdList->SetGraphicsRootConstantBufferView(2, matCbBase + objectIndex * sizeof(PerMaterialData));
        cmdList->SetGraphicsRootDescriptorTable(3, m_defaultWhiteTex.GetSRVHandle().gpu);
    };

    // Draw ground grid
    {
        PerObjectData objData;
        objData.world = Matrix::CreateTranslation(0.0f, 0.0f, 0.0f);
        m_perObjectCB.UpdateDataAtOffset(&objData, sizeof(objData), objectIndex * sizeof(PerObjectData));
        cmdList->SetGraphicsRootConstantBufferView(1, objCbBase + objectIndex * sizeof(PerObjectData));
        bindDefaultMaterial(Vector4(0.7f, 0.7f, 0.7f, 1.0f));
        objectIndex++;

        auto vbView = m_gridVB.GetVertexBufferView();
        auto ibView = m_gridIB.GetIndexBufferView();
        cmdList->IASetVertexBuffers(0, 1, &vbView);
        cmdList->IASetIndexBuffer(&ibView);
        cmdList->DrawIndexedInstanced(m_gridIndexCount, 1, 0, 0, 0);
    }

    // Draw cubes
    auto cubeVBView = m_cubeVB.GetVertexBufferView();
    auto cubeIBView = m_cubeIB.GetIndexBufferView();
    cmdList->IASetVertexBuffers(0, 1, &cubeVBView);
    cmdList->IASetIndexBuffer(&cubeIBView);

    for (const auto& cube : m_cubes) {
        PerObjectData objData;
        objData.world = cube.world;
        m_perObjectCB.UpdateDataAtOffset(&objData, sizeof(objData), objectIndex * sizeof(PerObjectData));
        cmdList->SetGraphicsRootConstantBufferView(1, objCbBase + objectIndex * sizeof(PerObjectData));
        bindDefaultMaterial(Vector4(0.7f, 0.7f, 0.7f, 1.0f));
        cmdList->DrawIndexedInstanced(m_cubeIndexCount, 1, 0, 0, 0);
        objectIndex++;
    }

    // Draw glTF model
    if (m_modelLoaded) {
        for (const auto& mesh : m_testModel.meshes) {
            for (const auto& prim : mesh.primitives) {
                if (prim.indexCount == 0) continue;

                // Per-object transform (identity for now)
                PerObjectData objData;
                objData.world = Matrix::CreateTranslation(0.0f, 0.0f, 0.0f);
                m_perObjectCB.UpdateDataAtOffset(&objData, sizeof(objData), objectIndex * sizeof(PerObjectData));
                cmdList->SetGraphicsRootConstantBufferView(1, objCbBase + objectIndex * sizeof(PerObjectData));

                // Per-material
                PerMaterialData matData;
                matData.baseColorFactor = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
                matData.hasTexture = 0;

                if (prim.materialIndex >= 0 &&
                    prim.materialIndex < static_cast<int>(m_testModel.materials.size())) {
                    const auto& mat = m_testModel.materials[prim.materialIndex];
                    matData.baseColorFactor = mat.baseColorFactor;

                    if (mat.baseColorTextureIndex >= 0 &&
                        mat.baseColorTextureIndex < static_cast<int>(m_testModel.textures.size())) {
                        matData.hasTexture = 1;
                        cmdList->SetGraphicsRootDescriptorTable(3,
                            m_testModel.textures[mat.baseColorTextureIndex].GetSRVHandle().gpu);
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

void CullingShowcase::OnImGui() {
    ImGui::Text("Camera Position: (%.1f, %.1f, %.1f)",
        m_camera.GetPosition().x, m_camera.GetPosition().y, m_camera.GetPosition().z);
    ImGui::Separator();
    ImGui::SliderFloat("Move Speed", &m_cameraController.moveSpeed, 1.0f, 50.0f);
    ImGui::SliderFloat("Look Sensitivity", &m_cameraController.lookSpeed, 0.001f, 0.01f);

    if (ImGui::Button("Reset Camera")) {
        m_camera.SetPosition(Vector3(0.0f, 5.0f, -15.0f));
        m_camera.SetLookAt(Vector3(0.0f, 0.0f, 0.0f));
        m_camera.UpdateViewMatrix();
    }

    ImGui::Separator();
    if (m_modelLoaded) {
        ImGui::Text("Model: %zu meshes, %zu materials, %zu textures",
            m_testModel.meshes.size(), m_testModel.materials.size(),
            m_testModel.textures.size());
    } else {
        ImGui::TextDisabled("No glTF model loaded");
    }
}

} // namespace showcase
