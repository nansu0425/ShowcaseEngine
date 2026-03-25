#include <showcase/editor/ImGuiLayer.h>

#include <showcase/core/FileSystem.h>
#include <showcase/core/Log.h>
#include <showcase/core/Profiler.h>
#include <showcase/graphics/CommandList.h>
#include <showcase/graphics/RenderContext.h>

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>

namespace showcase {

// ── Init / Shutdown ──────────────────────────────────────────────

bool ImGuiLayer::Init(HWND hwnd, RenderContext& ctx) {
    SE_ZONE_SCOPED_C(profile::kColorImGui);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Save ImGui layout to exe directory (not CWD)
    static std::string iniPath = GetExecutableDir() + "imgui.ini";
    io.IniFilename = iniPath.c_str();

    // Set dark theme
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 2.0f;

    // Allocate a descriptor for ImGui font texture
    auto handle = ctx.GetSrvHeap().Allocate();
    m_srvDescriptorIndex = handle.index;

    ImGui_ImplWin32_Init(hwnd);

    ImGui_ImplDX12_InitInfo initInfo = {};
    initInfo.Device = ctx.GetDevice().GetDevice();
    initInfo.CommandQueue = ctx.GetDirectQueue().GetQueue();
    initInfo.NumFramesInFlight = FrameResource::kNumFrames;
    initInfo.RTVFormat = ctx.GetSwapChain().GetFormat();
    initInfo.SrvDescriptorHeap = ctx.GetSrvHeap().GetHeap();
    initInfo.LegacySingleSrvCpuDescriptor = handle.cpu;
    initInfo.LegacySingleSrvGpuDescriptor = handle.gpu;
    ImGui_ImplDX12_Init(&initInfo);

    SE_LOG_INFO("ImGui initialized");
    return true;
}

void ImGuiLayer::Shutdown() {
    SE_ZONE_SCOPED_C(profile::kColorImGui);
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

// ── Frame management ──────────────────────────────────────────────

void ImGuiLayer::BeginFrame() {
    SE_ZONE_SCOPED_C(profile::kColorImGui);
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::EndFrame(CommandList& cmdList) {
    SE_ZONE_SCOPED_C(profile::kColorImGui);
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList.Get());
}

} // namespace showcase
