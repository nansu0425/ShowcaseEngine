#include <showcase/demo/ShowcaseManager.h>
#include <showcase/demo/ShowcaseRegistry.h>
#include <showcase/graphics/RenderContext.h>
#include <showcase/core/Log.h>
#include <imgui.h>

namespace showcase {

void ShowcaseManager::LoadShowcase(const std::string& name, RenderContext& ctx) {
    ctx.GetDirectQueue().Flush();
    UnloadCurrent();

    m_active = ShowcaseRegistry::Instance().Create(name);
    if (m_active) {
        m_activeName = name;
        m_active->OnLoad(ctx);
        SE_LOG_INFO("Loaded showcase: {}", name);
    }
}

void ShowcaseManager::UnloadCurrent() {
    if (m_active) {
        SE_LOG_INFO("Unloading showcase: {}", m_activeName);
        m_active->OnUnload();
        m_active.reset();
        m_activeName.clear();
    }
}

void ShowcaseManager::Update(float deltaTime, const Input& input, RenderContext& ctx) {
    if (!m_pendingName.empty()) {
        LoadShowcase(m_pendingName, ctx);
        m_pendingName.clear();
    }
    if (m_active) {
        m_active->OnUpdate(deltaTime, input);
    }
}

void ShowcaseManager::Render(RenderContext& ctx) {
    if (m_active) {
        m_active->OnRender(ctx);
    }
}

void ShowcaseManager::RenderUI() {
    // Showcase selector panel
    ImGui::SetNextWindowPos(ImVec2(10, 80), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Showcase Selector")) {
        const auto& entries = ShowcaseRegistry::Instance().GetAll();

        if (ImGui::BeginCombo("Demo", m_activeName.c_str())) {
            for (const auto& entry : entries) {
                bool isSelected = (m_activeName == entry.name);
                std::string label = entry.name + " [" + entry.category + "]";
                if (ImGui::Selectable(label.c_str(), isSelected)) {
                    if (!isSelected) {
                        m_pendingName = entry.name;
                    }
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Separator();

        // Active showcase's own UI
        if (m_active) {
            m_active->OnImGui();
        }
    }
    ImGui::End();
}

void ShowcaseManager::OnResize(uint32_t width, uint32_t height) {
    if (m_active) {
        m_active->OnResize(width, height);
    }
}

} // namespace showcase
