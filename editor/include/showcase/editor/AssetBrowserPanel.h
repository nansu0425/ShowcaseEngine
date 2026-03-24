#pragma once

#include <showcase/core/Platform.h>

#include <imgui.h>

#include <functional>
#include <string>
#include <vector>

namespace showcase {

struct AssetEntry {
    std::string relativePath; // e.g. "assets/models/BoxTextured.glb"
    std::string filename;     // e.g. "BoxTextured.glb"
    std::string extension;    // e.g. ".glb"
};

class AssetBrowserPanel {
public:
    void Init(void* ownerHwnd);
    void Render();
    void RefreshAssets();

    using AddToSceneCallback = std::function<void(const std::string& modelSource)>;
    void SetAddToSceneCallback(AddToSceneCallback cb) { m_addToSceneCallback = std::move(cb); }

private:
    void ScanDirectory();
    void ImportAsset();

    std::vector<AssetEntry> m_assets;
    std::string m_assetsPath; // runtime assets/ directory (exe-relative)
    void* m_ownerHwnd = nullptr;
    AddToSceneCallback m_addToSceneCallback;
    ImGuiTextFilter m_searchFilter;
    int m_selectedIndex = -1;
};

} // namespace showcase
