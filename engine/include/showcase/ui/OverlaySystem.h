#pragma once

#include <string>

namespace showcase {

class OverlaySystem {
public:
    void RenderFPSCounter(float fps, float deltaTime);
    void RenderShowcaseInfo(const std::string& name, const std::string& description);

    bool showFPS = true;
    bool showInfo = true;
};

} // namespace showcase
