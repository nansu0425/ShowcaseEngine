#pragma once

#include <string>
#include <cstdint>

namespace showcase {

class RenderContext;
class Input;

class IShowcase {
public:
    virtual ~IShowcase() = default;

    // Metadata
    virtual std::string GetName() const = 0;
    virtual std::string GetDescription() const = 0;
    virtual std::string GetCategory() const = 0;

    // Lifecycle
    virtual void OnLoad(RenderContext& ctx) = 0;
    virtual void OnUnload() = 0;
    virtual void OnResize(uint32_t width, uint32_t height) = 0;

    // Per-frame
    virtual void OnUpdate(float deltaTime, const Input& input) = 0;
    virtual void OnRender(RenderContext& ctx) = 0;

    // UI
    virtual void OnImGui() = 0;
    virtual void OnViewportToolbar() {}
};

} // namespace showcase
