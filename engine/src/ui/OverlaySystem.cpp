#include <showcase/ui/OverlaySystem.h>
#include <imgui.h>

namespace showcase {

void OverlaySystem::RenderFPSCounter(float fps, float deltaTime) {
    if (!showFPS) return;

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.5f);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize
                           | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing
                           | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

    if (ImGui::Begin("##FPS", nullptr, flags)) {
        ImGui::Text("FPS: %.1f", fps);
        ImGui::Text("Frame: %.2f ms", deltaTime * 1000.0f);
    }
    ImGui::End();
}

void OverlaySystem::RenderShowcaseInfo(const std::string& name, const std::string& description) {
    if (!showInfo || name.empty()) return;

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 10, 10), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.7f);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize
                           | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing
                           | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

    if (ImGui::Begin("##ShowcaseInfo", nullptr, flags)) {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", name.c_str());
        ImGui::Separator();
        ImGui::TextWrapped("%s", description.c_str());
    }
    ImGui::End();
}

} // namespace showcase
