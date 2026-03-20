#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <imgui.h>
#include <string>
#include <vector>
#include <mutex>

namespace showcase {

struct LogEntry {
    spdlog::level::level_enum level;
    std::string timestamp;
    std::string message;
};

class LogConsole {
public:
    void Init();
    void Render();
    void Clear();

    void AddEntry(spdlog::level::level_enum level, const std::string& timestamp, const std::string& message);

private:
    static ImVec4 GetLevelColor(spdlog::level::level_enum level);
    static const char* GetLevelLabel(spdlog::level::level_enum level);

    std::vector<LogEntry> m_entries;
    std::mutex m_mutex;
    bool m_autoScroll = true;
    bool m_open = true;
    int m_levelFilter = 0; // index into spdlog::level (0=trace)
    ImGuiTextFilter m_textFilter;
    static constexpr size_t MAX_ENTRIES = 2048;
};

} // namespace showcase
