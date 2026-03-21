#include "CullingShowcase.h"
#include <showcase/core/Log.h>
#include <showcase/core/Input.h>
#include <showcase/demo/ShowcaseRegistry.h>
#include <showcase/graphics/PipelineState.h>
#include <showcase/graphics/RenderContext.h>
#include <showcase/graphics/RootSignature.h>
#include <showcase/graphics/Model.h>
#include <showcase/graphics/CommandList.h>
#include <showcase/ui/Viewport.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <filesystem>

namespace showcase {

using namespace DirectX::SimpleMath;
using namespace DirectX;

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
    float selectionTint;
    float _pad[2];
};

// ── Procedural geometry ──────────────────────────────────────────────

void CullingShowcase::CreateGridModel(ID3D12Device* device, D3D12MA::Allocator* allocator) {
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

    m_gridModel.meshes.push_back(std::move(mesh));
    m_gridModel.localAABB = BoundingBox(XMFLOAT3(0, 0, 0), XMFLOAT3(halfExtent, 0.01f, halfExtent));
}

void CullingShowcase::CreateCubeModel(ID3D12Device* device, D3D12MA::Allocator* allocator) {
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

    m_cubeModel.meshes.push_back(std::move(mesh));
    m_cubeModel.localAABB = BoundingBox(XMFLOAT3(0, 0, 0), XMFLOAT3(0.5f, 0.5f, 0.5f));
}

// ── Ray picking ──────────────────────────────────────────────────────

int CullingShowcase::PickObject(int mouseX, int mouseY) const {
    float vpWidth = m_viewportMax.x - m_viewportMin.x;
    float vpHeight = m_viewportMax.y - m_viewportMin.y;
    if (vpWidth <= 0 || vpHeight <= 0) return -1;

    float localX = static_cast<float>(mouseX) - m_viewportMin.x;
    float localY = static_cast<float>(mouseY) - m_viewportMin.y;

    float ndcX = (localX / vpWidth) * 2.0f - 1.0f;
    float ndcY = 1.0f - (localY / vpHeight) * 2.0f;

    Matrix vp = m_camera.GetViewProjectionMatrix();
    Matrix invVP = vp.Invert();

    XMVECTOR nearPt = XMVector3TransformCoord(
        XMVectorSet(ndcX, ndcY, 0.0f, 1.0f), invVP);
    XMVECTOR farPt = XMVector3TransformCoord(
        XMVectorSet(ndcX, ndcY, 1.0f, 1.0f), invVP);

    XMVECTOR rayOrigin = nearPt;
    XMVECTOR rayDir = XMVector3Normalize(XMVectorSubtract(farPt, nearPt));

    int closestId = -1;
    float closestDist = FLT_MAX;

    for (const auto& obj : m_scene.GetObjects()) {
        float dist = 0.0f;
        if (obj.worldAABB.Intersects(rayOrigin, rayDir, dist)) {
            if (dist < closestDist) {
                closestDist = dist;
                closestId = static_cast<int>(obj.id);
            }
        }
    }

    return closestId;
}

// ── Lifecycle ────────────────────────────────────────────────────────

void CullingShowcase::OnLoad(RenderContext& ctx) {
    m_viewportPtr = ctx.GetViewport();
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

    // Create procedural models
    CreateGridModel(device, allocator);
    CreateCubeModel(device, allocator);

    // Build scene
    m_scene.Clear();
    m_scene.AddObject(&m_gridModel, "Ground Grid", Vector3(0, 0, 0));
    m_scene.AddObject(&m_cubeModel, "Cube 1", Vector3(0, 1, 0), Vector3(0, 0, 0), Vector3(2, 2, 2));
    m_scene.AddObject(&m_cubeModel, "Cube 2", Vector3(5, 0.75f, 3), Vector3(0, 0, 0), Vector3(1.5f, 1.5f, 1.5f));
    m_scene.AddObject(&m_cubeModel, "Cube 3", Vector3(-6, 2, -4), Vector3(0, 0, 0), Vector3(3, 4, 3));
    m_scene.AddObject(&m_cubeModel, "Cube 4", Vector3(8, 0.5f, -7), Vector3(0, 0, 0), Vector3(1, 1, 1));
    m_scene.AddObject(&m_cubeModel, "Cube 5", Vector3(-3, 0.5f, 8), Vector3(0, 0, 0), Vector3(2, 1, 2));

    // Try loading a glTF model from assets/models/
    {
        char exePath[MAX_PATH];
        GetModuleFileNameA(nullptr, exePath, MAX_PATH);
        std::filesystem::path modelDir = std::filesystem::path(exePath).parent_path() / "assets" / "models";

        if (std::filesystem::exists(modelDir)) {
            for (const auto& entry : std::filesystem::directory_iterator(modelDir)) {
                auto ext = entry.path().extension().string();
                if (ext == ".gltf" || ext == ".glb") {
                    m_modelLoaded = ModelLoader::LoadGLTF(
                        entry.path().string(), device, allocator,
                        ctx.GetDirectQueue(), ctx.GetSrvHeap(), m_testModel);
                    if (m_modelLoaded) {
                        SE_LOG_INFO("Loaded test model: {}", entry.path().filename().string());
                        m_scene.AddObject(&m_testModel, "glTF Model", Vector3(0, 0, 0));
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
    m_camera.SetPerspective(XM_PIDIV4, m_aspectRatio, 0.1f, 1000.0f);
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
    // Shutdown procedural model buffers
    for (auto& mesh : m_gridModel.meshes)
        for (auto& prim : mesh.primitives) {
            prim.vertexBuffer.Shutdown();
            prim.indexBuffer.Shutdown();
        }
    for (auto& mesh : m_cubeModel.meshes)
        for (auto& prim : mesh.primitives) {
            prim.vertexBuffer.Shutdown();
            prim.indexBuffer.Shutdown();
        }
    m_gridModel.meshes.clear();
    m_cubeModel.meshes.clear();

    m_scene.Clear();
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

    // Left-click picking (not during right-click camera rotation, not when using gizmo)
    if (input.IsMouseButtonPressed(0) && !input.IsMouseButtonDown(1)
        && m_viewportHovered && !ImGuizmo::IsOver()) {
        m_selectedObjectId = PickObject(input.GetMouseX(), input.GetMouseY());
    }

    // Gizmo operation shortcuts (Ctrl+T / Ctrl+R / Ctrl+S)
    if (input.IsKeyDown(VK_CONTROL)) {
        if (input.IsKeyPressed('T')) m_gizmoOperation = ImGuizmo::TRANSLATE;
        if (input.IsKeyPressed('R')) m_gizmoOperation = ImGuizmo::ROTATE;
        if (input.IsKeyPressed('S')) m_gizmoOperation = ImGuizmo::SCALE;
    }
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

    for (const auto& sceneObj : m_scene.GetObjects()) {
        if (!sceneObj.model) continue;

        bool isSelected = (static_cast<int>(sceneObj.id) == m_selectedObjectId);

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

void CullingShowcase::OnImGui() {
    ImGuizmo::BeginFrame();

    // -- Showcase info (rendered inside "Showcase Selector" window) --
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
    ImGui::Text("Objects: %zu", m_scene.GetObjectCount());
    if (m_modelLoaded) {
        ImGui::Text("Model: %zu meshes, %zu materials, %zu textures",
            m_testModel.meshes.size(), m_testModel.materials.size(),
            m_testModel.textures.size());
    } else {
        ImGui::TextDisabled("No glTF model loaded");
    }

    // -- Viewport hover detection (uses previous frame's image rect) --
    if (m_viewportPtr) {
        m_viewportMin = m_viewportPtr->GetImageMin();
        m_viewportMax = m_viewportPtr->GetImageMax();
        m_viewportHovered = ImGui::IsMouseHoveringRect(m_viewportMin, m_viewportMax, false);
    }

    // -- Gizmo rendering --
    auto* vpWindow = ImGui::FindWindowByName("Viewport");
    if (vpWindow && !vpWindow->Hidden && m_selectedObjectId > 0) {
        SceneObject* selected = m_scene.FindById(static_cast<uint32_t>(m_selectedObjectId));
        if (selected) {
            ImGuizmo::SetDrawlist(vpWindow->DrawList);
            ImGuizmo::SetRect(
                m_viewportMin.x, m_viewportMin.y,
                m_viewportMax.x - m_viewportMin.x,
                m_viewportMax.y - m_viewportMin.y);

            Matrix view = m_camera.GetViewMatrix();
            Matrix proj = m_camera.GetProjectionMatrix();
            Matrix world = selected->worldTransform;

            // ImGuizmo assumes RH coordinates. Our LH view matrix has Z basis
            // pointing into the scene; RH expects it pointing away. Negate view
            // column 2 and proj row 2 — the two flips cancel in view*proj (so
            // screen-space rendering is unchanged) while the camera-direction
            // extracted from inverse(view) matches RH expectations.
            view._13 = -view._13; view._23 = -view._23;
            view._33 = -view._33; view._43 = -view._43;
            proj._31 = -proj._31; proj._32 = -proj._32;
            proj._33 = -proj._33; proj._34 = -proj._34;

            float snap[3] = { 0.0f, 0.0f, 0.0f };
            if (m_useSnap) {
                float v = (m_gizmoOperation == ImGuizmo::ROTATE) ? m_snapRotate
                        : (m_gizmoOperation == ImGuizmo::SCALE)  ? m_snapScale
                        : m_snapTranslate;
                snap[0] = snap[1] = snap[2] = v;
            }

            ImGuizmo::Manipulate(
                &view.m[0][0], &proj.m[0][0],
                m_gizmoOperation, m_gizmoMode,
                &world.m[0][0], nullptr,
                m_useSnap ? snap : nullptr);

            if (ImGuizmo::IsUsing()) {
                Vector3 newPos, newScale;
                Quaternion newRot;
                world.Decompose(newScale, newRot, newPos);

                selected->position = newPos;
                selected->scale = newScale;

                auto euler = newRot.ToEuler();
                selected->rotation = Vector3(
                    XMConvertToDegrees(euler.x),
                    XMConvertToDegrees(euler.y),
                    XMConvertToDegrees(euler.z));

                selected->RecomputeWorldTransform();
                selected->UpdateAABB();
            }
        }
    }

    // -- Scene Hierarchy panel --
    if (ImGui::Begin("Scene Hierarchy")) {
        for (auto& obj : m_scene.GetObjects()) {
            bool isSelected = (static_cast<int>(obj.id) == m_selectedObjectId);
            if (ImGui::Selectable(obj.name.c_str(), isSelected)) {
                m_selectedObjectId = static_cast<int>(obj.id);
            }
        }
    }
    ImGui::End();

    // -- Inspector panel --
    if (ImGui::Begin("Inspector")) {
        SceneObject* selected = (m_selectedObjectId > 0) ? m_scene.FindById(static_cast<uint32_t>(m_selectedObjectId)) : nullptr;
        if (selected) {
            ImGui::Text("Name: %s", selected->name.c_str());
            ImGui::Text("ID: %u", selected->id);
            ImGui::Separator();

            bool changed = false;
            if (ImGui::DragFloat3("Position", &selected->position.x, 0.1f)) {
                changed = true;
            }
            if (ImGui::DragFloat3("Rotation", &selected->rotation.x, 1.0f)) {
                changed = true;
            }
            if (ImGui::DragFloat3("Scale", &selected->scale.x, 0.1f, 0.01f, 100.0f)) {
                changed = true;
            }

            if (changed) {
                selected->RecomputeWorldTransform();
                selected->UpdateAABB();
            }

        } else {
            ImGui::TextDisabled("No object selected");
        }
    }
    ImGui::End();

}

void CullingShowcase::OnViewportToolbar() {
    if (ImGui::RadioButton("Translate", m_gizmoOperation == ImGuizmo::TRANSLATE))
        m_gizmoOperation = ImGuizmo::TRANSLATE;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Move objects along axes (Ctrl+T)");
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", m_gizmoOperation == ImGuizmo::ROTATE))
        m_gizmoOperation = ImGuizmo::ROTATE;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rotate objects around axes (Ctrl+R)");
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", m_gizmoOperation == ImGuizmo::SCALE))
        m_gizmoOperation = ImGuizmo::SCALE;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Scale objects along axes (Ctrl+S)");

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    if (ImGui::RadioButton("Local", m_gizmoMode == ImGuizmo::LOCAL))
        m_gizmoMode = ImGuizmo::LOCAL;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Transform relative to object's own axes");
    ImGui::SameLine();
    if (ImGui::RadioButton("World", m_gizmoMode == ImGuizmo::WORLD))
        m_gizmoMode = ImGuizmo::WORLD;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Transform relative to world axes");

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    ImGui::Checkbox("Snap", &m_useSnap);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Snap transform values to fixed increments");
    if (m_useSnap) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        if (m_gizmoOperation == ImGuizmo::TRANSLATE)
            ImGui::DragFloat("##SnapVal", &m_snapTranslate, 0.1f, 0.1f, 10.0f);
        else if (m_gizmoOperation == ImGuizmo::ROTATE)
            ImGui::DragFloat("##SnapVal", &m_snapRotate, 1.0f, 1.0f, 90.0f);
        else
            ImGui::DragFloat("##SnapVal", &m_snapScale, 0.01f, 0.01f, 1.0f);
    }
}

} // namespace showcase
