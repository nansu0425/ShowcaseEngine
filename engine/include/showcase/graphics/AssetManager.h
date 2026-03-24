#pragma once

#include <showcase/graphics/Model.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace showcase {

class RenderContext;
class SceneRenderer;

class AssetManager {
public:
    void Init(RenderContext& ctx, SceneRenderer& sceneRenderer);
    void Shutdown();

    Model* LoadModel(const std::string& source);
    Model* GetModel(const std::string& source) const;
    Model* GetBuiltinCube() const;

    std::vector<std::string> GetAvailableSources() const;

private:
    RenderContext* m_ctx = nullptr;
    std::unordered_map<std::string, std::unique_ptr<Model>> m_models;
    Model* m_builtinCube = nullptr;
};

} // namespace showcase
