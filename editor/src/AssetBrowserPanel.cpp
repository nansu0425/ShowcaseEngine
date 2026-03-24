#include <showcase/editor/AssetBrowserPanel.h>

#include <showcase/core/Log.h>

#include <algorithm>
#include <filesystem>

namespace showcase {

// ── Init ──────────────────────────────────────────────────────────

void AssetBrowserPanel::Init(void* ownerHwnd) {
    m_ownerHwnd = ownerHwnd;
    m_assetsPath = GetExecutableDir() + "assets/";
    RefreshAssets();
}

// ── Directory scanning ────────────────────────────────────────────

void AssetBrowserPanel::RefreshAssets() {
    m_assets.clear();
    m_selectedIndex = -1;
    ScanDirectory();
}

void AssetBrowserPanel::ScanDirectory() {
    namespace fs = std::filesystem;

    std::error_code ec;
    if (!fs::exists(m_assetsPath, ec))
        return;

    for (const auto& entry : fs::recursive_directory_iterator(m_assetsPath, ec)) {
        if (!entry.is_regular_file())
            continue;

        std::string ext = entry.path().extension().string();
        // Lowercase the extension for comparison
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (ext != ".glb" && ext != ".gltf")
            continue;

        // Build relative path from exe directory (e.g. "assets/models/BoxTextured.glb")
        fs::path relPath = fs::relative(entry.path(), GetExecutableDir(), ec);
        if (ec)
            continue;

        std::string relStr = relPath.generic_string(); // use forward slashes

        AssetEntry asset;
        asset.relativePath = relStr;
        asset.filename = entry.path().filename().string();
        asset.extension = ext;
        m_assets.push_back(std::move(asset));
    }

    std::sort(m_assets.begin(), m_assets.end(),
              [](const AssetEntry& a, const AssetEntry& b) { return a.filename < b.filename; });
}

// ── Import ────────────────────────────────────────────────────────

void AssetBrowserPanel::ImportAsset() {
    const char* filter = "glTF Files (*.glb;*.gltf)\0*.glb;*.gltf\0All Files (*.*)\0*.*\0";
    std::string srcPath = OpenFileDialog(m_ownerHwnd, filter, "glb");
    if (srcPath.empty())
        return;

    namespace fs = std::filesystem;

    fs::path source(srcPath);
    std::string filename = source.filename().string();

    // Ensure models subdirectory exists
    fs::path destDir = fs::path(m_assetsPath) / "models";
    std::error_code ec;
    fs::create_directories(destDir, ec);

    fs::path destPath = destDir / filename;

    // Check for overwrite
    if (fs::exists(destPath)) {
        DialogResult result = ShowConfirmDialog(m_ownerHwnd, "Overwrite File",
                                                ("File \"" + filename + "\" already exists. Overwrite?").c_str());
        if (result != DialogResult::Yes)
            return;
    }

    fs::copy_file(source, destPath, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        SE_LOG_ERROR("Failed to import asset: {}", ec.message());
        return;
    }

    SE_LOG_INFO("Imported asset: {}", filename);
    RefreshAssets();
}

// ── ImGui rendering ───────────────────────────────────────────────

void AssetBrowserPanel::Render() {
    if (!ImGui::Begin("Asset Browser")) {
        ImGui::End();
        return;
    }

    // Toolbar
    if (ImGui::Button("Import"))
        ImportAsset();

    ImGui::SameLine();
    if (ImGui::Button("Refresh"))
        RefreshAssets();

    ImGui::SameLine();
    m_searchFilter.Draw("##AssetSearch", -1.0f);

    ImGui::Separator();

    // Asset list
    if (m_assets.empty()) {
        ImGui::TextDisabled("No assets found");
    } else {
        for (int i = 0; i < static_cast<int>(m_assets.size()); ++i) {
            const AssetEntry& entry = m_assets[i];

            if (!m_searchFilter.PassFilter(entry.filename.c_str()))
                continue;

            // Type label
            const char* typeLabel = (entry.extension == ".glb") ? "[GLB]" : "[GLTF]";
            ImGui::TextDisabled("%s", typeLabel);
            ImGui::SameLine();

            // Selectable item
            bool isSelected = (m_selectedIndex == i);
            if (ImGui::Selectable(entry.filename.c_str(), isSelected)) {
                m_selectedIndex = i;
            }

            // Double-click to add to scene
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                if (m_addToSceneCallback) {
                    m_addToSceneCallback("file:" + entry.relativePath);
                }
            }

            // Right-click context menu
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Add to Scene")) {
                    if (m_addToSceneCallback) {
                        m_addToSceneCallback("file:" + entry.relativePath);
                    }
                }
                ImGui::EndPopup();
            }

            // Tooltip
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
                ImGui::SetTooltip("%s", entry.relativePath.c_str());
            }
        }
    }

    ImGui::End();
}

} // namespace showcase
