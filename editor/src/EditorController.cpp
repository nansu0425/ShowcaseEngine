#include <showcase/editor/EditorController.h>

#include <showcase/core/Input.h>
#include <showcase/core/Profiler.h>
#include <showcase/editor/CommandHistory.h>
#include <showcase/editor/ViewportPanel.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cmath>

namespace showcase {

// ── Light Gizmo Helpers ──────────────────────────────────────────

static bool WorldToScreen(const Vector3& worldPos, const Matrix& viewProj, const ImVec2& vpMin, const ImVec2& vpMax,
                          ImVec2& out) {
    Vector4 clip = Vector4::Transform(Vector4(worldPos.x, worldPos.y, worldPos.z, 1.0f), viewProj);
    if (clip.w <= 0.0f)
        return false;
    float ndcX = clip.x / clip.w;
    float ndcY = clip.y / clip.w;
    out.x = vpMin.x + (ndcX * 0.5f + 0.5f) * (vpMax.x - vpMin.x);
    out.y = vpMin.y + (1.0f - (ndcY * 0.5f + 0.5f)) * (vpMax.y - vpMin.y);
    return true;
}

static void DrawLine3D(const Vector3& a, const Vector3& b, const Matrix& viewProj, const ImVec2& vpMin,
                       const ImVec2& vpMax, ImDrawList* drawList, ImU32 color, float thickness) {
    ImVec2 sa, sb;
    if (WorldToScreen(a, viewProj, vpMin, vpMax, sa) && WorldToScreen(b, viewProj, vpMin, vpMax, sb))
        drawList->AddLine(sa, sb, color, thickness);
}

// Draw a filled-triangle arrowhead in screen space (ImGuizmo style).
// tipWorld = arrow tip in 3D, baseWorld = a point back along the shaft.
static void DrawArrowHead3D(const Vector3& tipWorld, const Vector3& baseWorld, const Matrix& viewProj,
                            const ImVec2& vpMin, const ImVec2& vpMax, ImDrawList* drawList, ImU32 color,
                            float halfWidth) {
    ImVec2 sTip, sBase;
    if (!WorldToScreen(tipWorld, viewProj, vpMin, vpMax, sTip) ||
        !WorldToScreen(baseWorld, viewProj, vpMin, vpMax, sBase))
        return;

    // Screen-space direction and perpendicular (like ImGuizmo's DrawTranslationGizmo)
    ImVec2 dir(sTip.x - sBase.x, sTip.y - sBase.y);
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (len < 1e-4f)
        return;
    dir.x /= len;
    dir.y /= len;
    ImVec2 ortho(-dir.y * halfWidth, dir.x * halfWidth);

    drawList->AddTriangleFilled(sTip, ImVec2(sBase.x + ortho.x, sBase.y + ortho.y),
                                ImVec2(sBase.x - ortho.x, sBase.y - ortho.y), color);
}

static void DrawDirectionalLightGizmo(const SceneObject& obj, const Matrix& viewProj, const ImVec2& vpMin,
                                      const ImVec2& vpMax, ImDrawList* drawList) {
    const Vector3 pos = obj.position;

    // Compute constant-screen-size scale factor (same technique as ImGuizmo).
    // clip.w at the gizmo origin gives the perspective divisor — multiplying
    // world-space offsets by (clip.w * clipSpaceSize) keeps the gizmo the same
    // pixel size regardless of camera distance.
    Vector4 clip = Vector4::Transform(Vector4(pos.x, pos.y, pos.z, 1.0f), viewProj);
    if (clip.w <= 0.0f)
        return;
    constexpr float kGizmoClipSize = 0.1f;
    float s = clip.w * kGizmoClipSize; // world-space scale for constant screen size

    // Forward vector from world transform Z-axis row
    Vector3 forward(obj.worldTransform._31, obj.worldTransform._32, obj.worldTransform._33);
    forward.Normalize();

    // Build perpendicular basis
    Vector3 worldUp(0.0f, 1.0f, 0.0f);
    if (std::abs(forward.Dot(worldUp)) > 0.99f)
        worldUp = Vector3(1.0f, 0.0f, 0.0f);
    Vector3 right = worldUp.Cross(forward);
    right.Normalize();
    Vector3 up = forward.Cross(right);
    up.Normalize();

    constexpr ImU32 kColor = IM_COL32(255, 200, 50, 255);
    constexpr float kLineThickness = 3.0f;
    constexpr float kArrowHeadWidth = 6.0f;

    drawList->PushClipRect(vpMin, vpMax, true);

    // Center filled circle
    ImVec2 sCenter;
    if (WorldToScreen(pos, viewProj, vpMin, vpMax, sCenter))
        drawList->AddCircleFilled(sCenter, 5.0f, kColor);

    // Direction arrow shaft + filled arrowhead
    float arrowLen = 2.0f * s;
    float headFrac = 0.2f;
    Vector3 tip = pos + forward * arrowLen;
    Vector3 headBase = pos + forward * (arrowLen * (1.0f - headFrac));
    DrawLine3D(pos, headBase, viewProj, vpMin, vpMax, drawList, kColor, kLineThickness);
    DrawArrowHead3D(tip, headBase, viewProj, vpMin, vpMax, drawList, kColor, kArrowHeadWidth);

    // Circle ring perpendicular to forward
    constexpr int kCircleSegments = 12;
    float circleRadius = 0.3f * s;
    for (int i = 0; i < kCircleSegments; ++i) {
        float a0 = kTwoPi * static_cast<float>(i) / kCircleSegments;
        float a1 = kTwoPi * static_cast<float>(i + 1) / kCircleSegments;
        Vector3 p0 = pos + right * std::cos(a0) * circleRadius + up * std::sin(a0) * circleRadius;
        Vector3 p1 = pos + right * std::cos(a1) * circleRadius + up * std::sin(a1) * circleRadius;
        DrawLine3D(p0, p1, viewProj, vpMin, vpMax, drawList, kColor, kLineThickness);
    }

    // Emanating rays (6 rays from circle edge outward)
    constexpr int kRayCount = 6;
    float rayInner = 0.35f * s;
    float rayOuter = 1.0f * s;
    for (int i = 0; i < kRayCount; ++i) {
        float angle = kTwoPi * static_cast<float>(i) / kRayCount;
        Vector3 dir = right * std::cos(angle) + up * std::sin(angle);
        DrawLine3D(pos + dir * rayInner, pos + dir * rayOuter, viewProj, vpMin, vpMax, drawList, kColor,
                   kLineThickness);
    }

    drawList->PopClipRect();
}

// ── Update ────────────────────────────────────────────────────────

void EditorController::Update(const EditorUpdateDesc& desc) {
    SE_ZONE_SCOPED_C(profile::kColorUpdate);
    const Input& input = *desc.input;
    Scene& scene = *desc.scene;
    SceneRenderer& renderer = *desc.renderer;
    ViewportPanel& viewport = *desc.viewport;
    RenderContext& renderContext = *desc.renderContext;

    // Check for completed GPU pick result from previous frame
    if (m_pickPending && renderer.IsPickComplete(renderContext)) {
        m_selectedObjectId = renderer.GetPickResult(renderContext);
        m_pickPending = false;
    }

    // Left-click picking (not during right-click camera rotation, not when using gizmo)
    if (input.IsMouseButtonPressed(0) && !input.IsMouseButtonDown(1) && m_viewportHovered && !ImGuizmo::IsOver()) {
        // Convert screen coordinates to ID RT pixel coordinates
        float vpWidth = m_viewportMax.x - m_viewportMin.x;
        float vpHeight = m_viewportMax.y - m_viewportMin.y;
        if (vpWidth > 0 && vpHeight > 0) {
            float localX = static_cast<float>(input.GetMouseX()) - m_viewportMin.x;
            float localY = static_cast<float>(input.GetMouseY()) - m_viewportMin.y;
            float normX = localX / vpWidth;
            float normY = localY / vpHeight;

            int pixelX = static_cast<int>(normX * viewport.GetWidth());
            int pixelY = static_cast<int>(normY * viewport.GetHeight());

            pixelX = std::clamp(pixelX, 0, static_cast<int>(viewport.GetWidth()) - 1);
            pixelY = std::clamp(pixelY, 0, static_cast<int>(viewport.GetHeight()) - 1);

            renderer.RequestPick(pixelX, pixelY);
            m_pickPending = true;
        }
    }

    // Gizmo shortcuts (only when right-click not held, to avoid camera movement conflict)
    if (!input.IsMouseButtonDown(1)) {
        if (input.IsKeyPressed('W')) {
            m_gizmoOperation = ImGuizmo::TRANSLATE;
        }
        if (input.IsKeyPressed('E')) {
            m_gizmoOperation = ImGuizmo::ROTATE;
        }
        if (input.IsKeyPressed('R')) {
            m_gizmoOperation = ImGuizmo::SCALE;
        }
        if (input.IsKeyPressed('X')) {
            m_gizmoMode = (m_gizmoMode == ImGuizmo::LOCAL) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
        }
        if (input.IsKeyPressed(Key::kDelete) && m_selectedObjectId > 0) {
            if (m_commandHistory) {
                m_commandHistory->ExecuteCommand(std::make_unique<RemoveObjectCommand>(
                    scene, m_selectedObjectId, static_cast<uint32_t>(m_selectedObjectId)));
            }
        }
    }
}

// ── Primitive Details (Model Info panel helper) ──────────────────

static void RenderPrimitiveDetails(const MeshPrimitive& prim) {
    ImGui::Text("Triangles: %u", prim.indexCount / 3);

    if (prim.material) {
        char matLabel[256];
        snprintf(matLabel, sizeof(matLabel), "Material [%d] %s", prim.materialIndex,
                 prim.material->name.empty() ? "(unnamed)" : prim.material->name.c_str());
        if (ImGui::TreeNode(matLabel)) {
            const auto& c = prim.material->baseColorFactor;
            ImGui::ColorButton("##baseColor", ImVec4(c.x, c.y, c.z, c.w), ImGuiColorEditFlags_NoTooltip);
            ImGui::SameLine();
            ImGui::Text("Base Color: (%.2f, %.2f, %.2f, %.2f)", c.x, c.y, c.z, c.w);

            if (prim.material->baseColorTexture) {
                uint32_t w = prim.material->baseColorTexture->GetWidth();
                uint32_t h = prim.material->baseColorTexture->GetHeight();
                const auto& uri = prim.material->baseColorTexture->GetSourceURI();
                if (!uri.empty()) {
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Texture: %ux%u [%s]", w, h, uri.c_str());
                } else {
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "Texture: %ux%u", w, h);
                }

                // Texture preview
                D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = prim.material->baseColorTexture->GetSRVHandle().gpu;
                float previewSize = 128.0f;
                ImGui::Image((ImTextureID)gpuHandle.ptr, ImVec2(previewSize, previewSize));
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Texture: No Texture");
            }

            if (prim.material->alphaMode == AlphaMode::Mask) {
                ImGui::Text("Alpha: Mask (cutoff: %.2f)", prim.material->alphaCutoff);
            } else if (prim.material->alphaMode == AlphaMode::Blend) {
                ImGui::Text("Alpha: Blend");
            } else {
                ImGui::Text("Alpha: Opaque");
            }

            if (prim.material->doubleSided)
                ImGui::Text("Double-sided: Yes");

            ImGui::TreePop();
        }
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "No Material");
    }
}

// ── UI ────────────────────────────────────────────────────────────

void EditorController::RenderUI(Scene& scene, ViewportPanel& viewport) {
    SE_ZONE_SCOPED_C(profile::kColorImGui);
    Camera& camera = viewport.GetCamera();

    ImGuizmo::BeginFrame();

    // -- Viewport hover detection (uses previous frame's image rect) --
    m_viewportMin = viewport.GetImageMin();
    m_viewportMax = viewport.GetImageMax();
    m_viewportHovered = ImGui::IsMouseHoveringRect(m_viewportMin, m_viewportMax, false);

    // Clear any ImGui widget focus when right-clicking viewport (e.g., console input)
    if (m_viewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        ImGui::FocusWindow(nullptr);
    }

    // -- Gizmo rendering --
    auto* vpWindow = ImGui::FindWindowByName("Viewport");
    if (vpWindow && !vpWindow->Hidden && m_selectedObjectId > 0) {
        SceneObject* selected = scene.FindById(static_cast<uint32_t>(m_selectedObjectId));
        if (selected) {
            ImGuizmo::SetDrawlist(vpWindow->DrawList);
            ImGuizmo::SetRect(m_viewportMin.x, m_viewportMin.y, m_viewportMax.x - m_viewportMin.x,
                              m_viewportMax.y - m_viewportMin.y);

            Matrix view = camera.GetViewMatrix();
            Matrix proj = camera.GetProjectionMatrix();
            Matrix world = selected->worldTransform;

            // ImGuizmo assumes RH coordinates. Our LH view matrix has Z basis
            // pointing into the scene; RH expects it pointing away. Negate view
            // column 2 and proj row 2 — the two flips cancel in view*proj (so
            // screen-space rendering is unchanged) while the camera-direction
            // extracted from inverse(view) matches RH expectations.
            view._13 = -view._13;
            view._23 = -view._23;
            view._33 = -view._33;
            view._43 = -view._43;
            proj._31 = -proj._31;
            proj._32 = -proj._32;
            proj._33 = -proj._33;
            proj._34 = -proj._34;

            float snap[3] = {0.0f, 0.0f, 0.0f};
            if (m_useSnap) {
                float v = (m_gizmoOperation == ImGuizmo::ROTATE)  ? m_snapRotate
                          : (m_gizmoOperation == ImGuizmo::SCALE) ? m_snapScale
                                                                  : m_snapTranslate;
                snap[0] = snap[1] = snap[2] = v;
            }

            ImGuizmo::Manipulate(&view.m[0][0], &proj.m[0][0], m_gizmoOperation, m_gizmoMode, &world.m[0][0], nullptr,
                                 m_useSnap ? snap : nullptr);

            if (ImGuizmo::IsUsing()) {
                if (!m_gizmoDragging) {
                    m_gizmoDragging = true;
                    m_gizmoStartPos = selected->position;
                    m_gizmoStartRot = selected->rotation;
                    m_gizmoStartScale = selected->scale;
                }

                Vector3 newPos, newScale;
                Quaternion newRot;
                if (world.Decompose(newScale, newRot, newPos)) {
                    selected->position = newPos;
                    selected->scale = ClampScale(newScale);

                    Vector3 euler = newRot.ToEuler();
                    selected->rotation = Vector3(ToDegrees(euler.x), ToDegrees(euler.y), ToDegrees(euler.z));

                    selected->RecomputeWorldTransform();
                    selected->UpdateAABB();

                    if (m_dirtyCallback)
                        m_dirtyCallback();
                }
            } else if (m_gizmoDragging) {
                m_gizmoDragging = false;
                if (m_commandHistory) {
                    m_commandHistory->RecordCommand(std::make_unique<TransformCommand>(
                        TransformCommandDesc{&scene, selected->id, m_gizmoStartPos, m_gizmoStartRot, m_gizmoStartScale,
                                             selected->position, selected->rotation, selected->scale}));
                }
            }

            // Draw directional light gizmo
            if (selected->HasLight() && selected->lightComp->type == LightType::Directional) {
                Matrix vp = camera.GetViewProjectionMatrix();
                DrawDirectionalLightGizmo(*selected, vp, m_viewportMin, m_viewportMax, vpWindow->DrawList);
            }
        }
    }

    // -- Scene Hierarchy panel --
    if (ImGui::Begin("Scene Hierarchy")) {
        if (ImGui::Button("+")) {
            if (m_addObjectCallback) {
                SceneObject* newObj = m_addObjectCallback("");
                if (newObj && m_commandHistory) {
                    m_commandHistory->ExecuteCommand(
                        std::make_unique<AddObjectCommand>(scene, m_selectedObjectId, *newObj));
                }
            }
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Add object to scene");
        }
        ImGui::SameLine();
        const bool hasSelection = m_selectedObjectId > 0;
        if (!hasSelection)
            ImGui::BeginDisabled();
        if (ImGui::Button("-")) {
            if (m_commandHistory) {
                m_commandHistory->ExecuteCommand(std::make_unique<RemoveObjectCommand>(
                    scene, m_selectedObjectId, static_cast<uint32_t>(m_selectedObjectId)));
            }
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Remove selected object (Del)");
        }
        if (!hasSelection)
            ImGui::EndDisabled();

        ImGui::Separator();

        for (auto& obj : scene.GetObjects()) {
            ImGui::PushID(static_cast<int>(obj.id));
            bool isSelected = (static_cast<int>(obj.id) == m_selectedObjectId);
            if (ImGui::Selectable(obj.name.c_str(), isSelected)) {
                m_selectedObjectId = static_cast<int>(obj.id);
            }
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Delete")) {
                    if (m_commandHistory) {
                        m_commandHistory->ExecuteCommand(
                            std::make_unique<RemoveObjectCommand>(scene, m_selectedObjectId, obj.id));
                    }
                    ImGui::EndPopup();
                    ImGui::PopID();
                    break;
                }
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
    }
    ImGui::End();

    // -- Inspector panel --
    if (ImGui::Begin("Inspector")) {
        SceneObject* selected =
            (m_selectedObjectId > 0) ? scene.FindById(static_cast<uint32_t>(m_selectedObjectId)) : nullptr;
        if (selected) {
            char nameBuf[256];
            strncpy_s(nameBuf, selected->name.c_str(), sizeof(nameBuf) - 1);
            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
                selected->name = nameBuf;
                if (m_dirtyCallback)
                    m_dirtyCallback();
            }
            if (ImGui::IsItemActivated()) {
                m_dragStartName = selected->name;
            }
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                if (m_commandHistory && selected->name != m_dragStartName) {
                    m_commandHistory->RecordCommand(
                        std::make_unique<RenameCommand>(scene, selected->id, m_dragStartName, selected->name));
                }
            }
            ImGui::SameLine();
            ImGui::TextDisabled("ID: %u", selected->id);
            ImGui::Separator();

            // ── Transform (always shown) ──
            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                bool changed = false;

                if (ImGui::DragFloat3("Position", &selected->position.x, 0.1f)) {
                    changed = true;
                }
                if (ImGui::IsItemActivated()) {
                    m_dragStartPos = selected->position;
                    m_dragStartRot = selected->rotation;
                    m_dragStartScale = selected->scale;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    if (m_commandHistory) {
                        m_commandHistory->RecordCommand(std::make_unique<TransformCommand>(
                            TransformCommandDesc{&scene, selected->id, m_dragStartPos, m_dragStartRot, m_dragStartScale,
                                                 selected->position, selected->rotation, selected->scale}));
                    }
                }

                if (ImGui::DragFloat3("Rotation", &selected->rotation.x, 1.0f)) {
                    changed = true;
                }
                if (ImGui::IsItemActivated()) {
                    m_dragStartPos = selected->position;
                    m_dragStartRot = selected->rotation;
                    m_dragStartScale = selected->scale;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    if (m_commandHistory) {
                        m_commandHistory->RecordCommand(std::make_unique<TransformCommand>(
                            TransformCommandDesc{&scene, selected->id, m_dragStartPos, m_dragStartRot, m_dragStartScale,
                                                 selected->position, selected->rotation, selected->scale}));
                    }
                }

                if (ImGui::DragFloat3("Scale", &selected->scale.x, 0.1f, 0.01f, 100.0f)) {
                    selected->scale = ClampScale(selected->scale);
                    changed = true;
                }
                if (ImGui::IsItemActivated()) {
                    m_dragStartPos = selected->position;
                    m_dragStartRot = selected->rotation;
                    m_dragStartScale = selected->scale;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    if (m_commandHistory) {
                        m_commandHistory->RecordCommand(std::make_unique<TransformCommand>(
                            TransformCommandDesc{&scene, selected->id, m_dragStartPos, m_dragStartRot, m_dragStartScale,
                                                 selected->position, selected->rotation, selected->scale}));
                    }
                }

                if (changed) {
                    selected->RecomputeWorldTransform();
                    selected->UpdateAABB();
                    if (m_dirtyCallback)
                        m_dirtyCallback();
                }
            }

            // ── Mesh Component ──
            if (selected->modelComp.has_value()) {
                bool headerOpen = true;
                if (ImGui::CollapsingHeader("Model Component", &headerOpen, ImGuiTreeNodeFlags_DefaultOpen)) {
                    std::string currentSource = selected->modelComp->modelSource;
                    const char* preview = currentSource.empty() ? "(none)" : currentSource.c_str();

                    if (ImGui::BeginCombo("Model", preview)) {
                        // "(none)" option to clear model
                        if (ImGui::Selectable("(none)", currentSource.empty())) {
                            if (m_commandHistory) {
                                m_commandHistory->ExecuteCommand(
                                    std::make_unique<ChangeModelCommand>(ChangeModelCommandDesc{
                                        &scene, selected->id, currentSource, "", m_resolveModelCallback}));
                            }
                        }

                        // List available assets
                        if (m_assetListCallback) {
                            auto sources = m_assetListCallback();
                            for (const auto& src : sources) {
                                bool isCurrent = (src == currentSource);
                                if (ImGui::Selectable(src.c_str(), isCurrent)) {
                                    if (m_commandHistory) {
                                        m_commandHistory->ExecuteCommand(
                                            std::make_unique<ChangeModelCommand>(ChangeModelCommandDesc{
                                                &scene, selected->id, currentSource, src, m_resolveModelCallback}));
                                    }
                                }
                                if (isCurrent)
                                    ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                    // ── Model Info ──
                    if (selected->modelComp->model) {
                        const auto* model = selected->modelComp->model;
                        ImGui::Spacing();
                        ImGui::SeparatorText("Model Info");

                        // Reset hover state each frame
                        m_hoveredMeshIdx = -1;
                        m_hoveredPrimIdx = -1;

                        uint32_t totalPrims = 0;
                        for (const auto& mesh : model->meshes) {
                            totalPrims += static_cast<uint32_t>(mesh.primitives.size());
                        }
                        ImGui::Text("Meshes: %zu | Primitives: %u", model->meshes.size(), totalPrims);

                        for (size_t mi = 0; mi < model->meshes.size(); ++mi) {
                            const auto& mesh = model->meshes[mi];
                            const char* meshName = mesh.name.empty() ? "(unnamed)" : mesh.name.c_str();
                            char meshLabel[256];
                            snprintf(meshLabel, sizeof(meshLabel), "[%zu] %s (%zu primitive%s)", mi, meshName,
                                     mesh.primitives.size(), mesh.primitives.size() == 1 ? "" : "s");

                            if (ImGui::TreeNode(meshLabel)) {
                                for (size_t pi = 0; pi < mesh.primitives.size(); ++pi) {
                                    ImGui::PushID(static_cast<int>(pi));
                                    char primLabel[128];
                                    snprintf(primLabel, sizeof(primLabel), "Primitive %zu", pi);
                                    bool open = ImGui::TreeNode(primLabel);
                                    if (ImGui::IsItemHovered()) {
                                        m_hoveredMeshIdx = static_cast<int>(mi);
                                        m_hoveredPrimIdx = static_cast<int>(pi);
                                    }
                                    if (open) {
                                        RenderPrimitiveDetails(mesh.primitives[pi]);
                                        ImGui::TreePop();
                                    }
                                    ImGui::PopID();
                                }
                                ImGui::TreePop();
                            }
                        }
                    }

                    // ── Material Override ──
                    ImGui::Spacing();
                    ImGui::SeparatorText("Material Override");

                    bool hasOverride = selected->modelComp->baseColor.has_value();
                    if (ImGui::Checkbox("Base Color Override", &hasOverride)) {
                        std::optional<Vector4> oldColor = selected->modelComp->baseColor;
                        std::optional<Vector4> newColor =
                            hasOverride ? std::optional(Vector4(0.7f, 0.7f, 0.7f, 1.0f)) : std::nullopt;
                        if (m_commandHistory) {
                            m_commandHistory->ExecuteCommand(std::make_unique<ChangeBaseColorCommand>(
                                ChangeBaseColorCommandDesc{&scene, selected->id, oldColor, newColor}));
                        }
                    }
                    if (selected->modelComp->baseColor.has_value()) {
                        float color[3] = {selected->modelComp->baseColor->x, selected->modelComp->baseColor->y,
                                          selected->modelComp->baseColor->z};
                        if (ImGui::ColorEdit3("Color", color)) {
                            selected->modelComp->baseColor = Vector4(color[0], color[1], color[2], 1.0f);
                            if (m_dirtyCallback)
                                m_dirtyCallback();
                        }
                        if (ImGui::IsItemActivated()) {
                            m_dragStartColor = selected->modelComp->baseColor;
                        }
                        if (ImGui::IsItemDeactivatedAfterEdit()) {
                            if (m_commandHistory) {
                                m_commandHistory->RecordCommand(
                                    std::make_unique<ChangeBaseColorCommand>(ChangeBaseColorCommandDesc{
                                        &scene, selected->id, m_dragStartColor, selected->modelComp->baseColor}));
                            }
                        }
                    }
                }

                // Remove component if header close button was clicked
                if (!headerOpen) {
                    if (m_commandHistory) {
                        m_commandHistory->ExecuteCommand(std::make_unique<AddComponentCommand>(
                            AddComponentCommandDesc{&scene, selected->id, selected->modelComp, std::nullopt}));
                    }
                }
            }

            // ── Light Component ──
            if (selected->lightComp.has_value()) {
                bool lightHeaderOpen = true;
                if (ImGui::CollapsingHeader("Light Component", &lightHeaderOpen, ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& light = *selected->lightComp;

                    float color[3] = {light.color.x, light.color.y, light.color.z};
                    if (ImGui::ColorEdit3("Light Color", color)) {
                        light.color = Vector3(color[0], color[1], color[2]);
                        if (m_dirtyCallback)
                            m_dirtyCallback();
                    }
                    if (ImGui::IsItemActivated()) {
                        m_dragStartLightProps = light;
                    }
                    if (ImGui::IsItemDeactivatedAfterEdit()) {
                        if (m_commandHistory) {
                            m_commandHistory->RecordCommand(std::make_unique<ChangeLightPropertiesCommand>(
                                ChangeLightPropertiesCommandDesc{&scene, selected->id, m_dragStartLightProps, light}));
                        }
                    }

                    DragLightProperty(
                        {"Intensity", &light.intensity, 0.01f, 0.0f, 10.0f, &light, &scene, selected->id});
                    DragLightProperty(
                        {"Ambient", &light.ambientIntensity, 0.01f, 0.0f, 1.0f, &light, &scene, selected->id});
                    DragLightProperty(
                        {"Specular Power", &light.specularPower, 1.0f, 1.0f, 256.0f, &light, &scene, selected->id});

                    ImGui::TextDisabled("Direction controlled by object rotation");
                }

                if (!lightHeaderOpen) {
                    if (m_commandHistory) {
                        m_commandHistory->ExecuteCommand(std::make_unique<AddLightComponentCommand>(
                            AddLightComponentCommandDesc{&scene, selected->id, selected->lightComp, std::nullopt}));
                    }
                }
            }

            // ── Add Component button ──
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("+ Add Component")) {
                ImGui::OpenPopup("AddComponentPopup");
            }
            if (ImGui::BeginPopup("AddComponentPopup")) {
                bool allAdded = true;
                if (!selected->modelComp.has_value()) {
                    allAdded = false;
                    if (ImGui::MenuItem("Model")) {
                        if (m_commandHistory) {
                            m_commandHistory->ExecuteCommand(std::make_unique<AddComponentCommand>(
                                AddComponentCommandDesc{&scene, selected->id, std::nullopt, ModelComponent{}}));
                        }
                    }
                }
                if (!selected->lightComp.has_value()) {
                    allAdded = false;
                    if (ImGui::MenuItem("Directional Light")) {
                        if (m_commandHistory) {
                            m_commandHistory->ExecuteCommand(std::make_unique<AddLightComponentCommand>(
                                AddLightComponentCommandDesc{&scene, selected->id, std::nullopt, LightComponent{}}));
                        }
                    }
                }
                if (allAdded) {
                    ImGui::TextDisabled("All components added");
                }
                ImGui::EndPopup();
            }

        } else {
            ImGui::TextDisabled("No object selected");
        }
    }
    ImGui::End();
}

// ── Light property drag helper ───────────────────────────────────

void EditorController::DragLightProperty(const DragLightPropertyDesc& desc) {
    if (ImGui::DragFloat(desc.label, desc.value, desc.speed, desc.min, desc.max)) {
        if (m_dirtyCallback)
            m_dirtyCallback();
    }
    if (ImGui::IsItemActivated()) {
        m_dragStartLightProps = *desc.light;
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        if (m_commandHistory) {
            m_commandHistory->RecordCommand(std::make_unique<ChangeLightPropertiesCommand>(
                ChangeLightPropertiesCommandDesc{desc.scene, desc.objectId, m_dragStartLightProps, *desc.light}));
        }
    }
}

// ── Toolbar ───────────────────────────────────────────────────────

void EditorController::RenderToolbar(ViewportPanel& viewport) {
    Camera& camera = viewport.GetCamera();

    if (ImGui::RadioButton("Translate", m_gizmoOperation == ImGuizmo::TRANSLATE)) {
        m_gizmoOperation = ImGuizmo::TRANSLATE;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Move objects along axes (W)");
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Rotate", m_gizmoOperation == ImGuizmo::ROTATE)) {
        m_gizmoOperation = ImGuizmo::ROTATE;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Rotate objects around axes (E)");
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Scale", m_gizmoOperation == ImGuizmo::SCALE)) {
        m_gizmoOperation = ImGuizmo::SCALE;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Scale objects along axes (R)");
    }

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    if (ImGui::RadioButton("Local", m_gizmoMode == ImGuizmo::LOCAL)) {
        m_gizmoMode = ImGuizmo::LOCAL;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Transform relative to object's own axes (X)");
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("World", m_gizmoMode == ImGuizmo::WORLD)) {
        m_gizmoMode = ImGuizmo::WORLD;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Transform relative to world axes (X)");
    }

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    ImGui::Checkbox("Snap", &m_useSnap);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Snap transform values to fixed increments");
    }
    if (m_useSnap) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        if (m_gizmoOperation == ImGuizmo::TRANSLATE) {
            ImGui::DragFloat("##SnapVal", &m_snapTranslate, 0.1f, 0.1f, 10.0f);
        } else if (m_gizmoOperation == ImGuizmo::ROTATE) {
            ImGui::DragFloat("##SnapVal", &m_snapRotate, 1.0f, 1.0f, 90.0f);
        } else {
            ImGui::DragFloat("##SnapVal", &m_snapScale, 0.01f, 0.01f, 1.0f);
        }
    }

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    if (ImGui::Button("Camera")) {
        ImGui::OpenPopup("CameraSettings");
    }

    if (ImGui::BeginPopup("CameraSettings")) {
        ImGui::Text("Position: (%.1f, %.1f, %.1f)", camera.GetPosition().x, camera.GetPosition().y,
                    camera.GetPosition().z);
        ImGui::SliderFloat("Move Speed", &viewport.cameraMoveSpeed, 1.0f, 50.0f);
        ImGui::SliderFloat("Look Sensitivity", &viewport.cameraLookSpeed, 0.001f, 0.01f);
        ImGui::EndPopup();
    }
}

} // namespace showcase
