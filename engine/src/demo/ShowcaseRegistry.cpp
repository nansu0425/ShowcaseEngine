#include <showcase/demo/ShowcaseRegistry.h>
#include <showcase/demo/IShowcase.h>
#include <showcase/core/Log.h>

namespace showcase {

ShowcaseRegistry& ShowcaseRegistry::Instance() {
    static ShowcaseRegistry instance;
    return instance;
}

void ShowcaseRegistry::Register(const std::string& name, const std::string& category,
                                 std::function<std::unique_ptr<IShowcase>()> factory) {
    m_entries.push_back({name, category, std::move(factory)});
    if (Log::GetLogger()) {
        SE_LOG_INFO("Showcase registered: {} [{}]", name, category);
    }
}

std::unique_ptr<IShowcase> ShowcaseRegistry::Create(const std::string& name) const {
    for (const auto& entry : m_entries) {
        if (entry.name == name) {
            return entry.factory();
        }
    }
    SE_LOG_WARN("Showcase not found: {}", name);
    return nullptr;
}

} // namespace showcase
