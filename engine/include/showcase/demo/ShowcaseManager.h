#pragma once

#include <showcase/demo/IShowcase.h>
#include <showcase/core/Input.h>
#include <memory>
#include <string>

namespace showcase {

class RenderContext;

class ShowcaseManager {
public:
    void LoadShowcase(const std::string& name, RenderContext& ctx);
    void UnloadCurrent();

    void Update(float deltaTime, const Input& input, RenderContext& ctx);
    void Render(RenderContext& ctx);
    void RenderUI();
    void RenderToolbar();
    void OnResize(uint32_t width, uint32_t height);

    IShowcase* GetActive() const { return m_active.get(); }
    const std::string& GetActiveName() const { return m_activeName; }

private:
    std::unique_ptr<IShowcase> m_active;
    std::string m_activeName;
    std::string m_pendingName;
};

} // namespace showcase
