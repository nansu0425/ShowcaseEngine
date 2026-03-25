#include <showcase/graphics/AssetManager.h>

#include <showcase/core/Log.h>
#include <showcase/core/Platform.h>
#include <showcase/core/Profiler.h>
#include <showcase/graphics/RenderContext.h>
#include <showcase/graphics/SceneRenderer.h>

namespace showcase {

void AssetManager::Init(RenderContext& ctx, SceneRenderer& sceneRenderer) {
    m_ctx = &ctx;

    // Create builtin cube model
    std::unique_ptr<Model> cube = std::make_unique<Model>();
    sceneRenderer.CreateCubeModel(ctx, *cube);
    m_builtinCube = cube.get();
    m_models["builtin:cube"] = std::move(cube);
}

void AssetManager::Shutdown() {
    for (auto& [key, model] : m_models) {
        model->Shutdown(*m_ctx);
    }
    m_models.clear();
    m_builtinCube = nullptr;
}

Model* AssetManager::LoadModel(const std::string& source) {
    SE_ZONE_SCOPED;
    // Check if already loaded
    auto it = m_models.find(source);
    if (it != m_models.end()) {
        return it->second.get();
    }

    // Try loading file-based models (file:relative/path.gltf)
    if (source.rfind("file:", 0) == 0) {
        std::string relPath = source.substr(5);
        std::string absPath = GetExecutableDir() + relPath;

        std::unique_ptr<Model> model = std::make_unique<Model>();
        if (ModelLoader::LoadGLTF(*m_ctx, absPath, *model)) {
            SE_LOG_INFO("Loaded model: {}", relPath);
            Model* ptr = model.get();
            m_models[source] = std::move(model);
            return ptr;
        }

        SE_LOG_WARN("Failed to load model: {}", absPath);
    }

    return nullptr;
}

Model* AssetManager::GetModel(const std::string& source) const {
    auto it = m_models.find(source);
    return (it != m_models.end()) ? it->second.get() : nullptr;
}

Model* AssetManager::GetBuiltinCube() const {
    return m_builtinCube;
}

std::vector<std::string> AssetManager::GetAvailableSources() const {
    std::vector<std::string> sources;
    sources.reserve(m_models.size());
    for (const auto& [key, _] : m_models) {
        sources.push_back(key);
    }
    return sources;
}

} // namespace showcase
