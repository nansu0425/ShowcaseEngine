#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace showcase {

class IShowcase;

struct ShowcaseEntry {
    std::string name;
    std::string category;
    std::function<std::unique_ptr<IShowcase>()> factory;
};

class ShowcaseRegistry {
public:
    static ShowcaseRegistry& Instance();

    void Register(const std::string& name, const std::string& category,
                  std::function<std::unique_ptr<IShowcase>()> factory);

    const std::vector<ShowcaseEntry>& GetAll() const { return m_entries; }
    std::unique_ptr<IShowcase> Create(const std::string& name) const;

private:
    ShowcaseRegistry() = default;
    std::vector<ShowcaseEntry> m_entries;
};

// Macro for auto-registration — place in .cpp of each showcase
#define REGISTER_SHOWCASE(ClassName) \
    namespace { \
    static bool ClassName##_registered = [] { \
        auto temp = std::make_unique<ClassName>(); \
        ::showcase::ShowcaseRegistry::Instance().Register( \
            temp->GetName(), \
            temp->GetCategory(), \
            [] { return std::make_unique<ClassName>(); }); \
        return true; \
    }(); \
    }

} // namespace showcase
