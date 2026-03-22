#pragma once

#include <showcase/core/LogListener.h>

#include <imgui.h>

#include <algorithm>
#include <string>
#include <vector>
#include <mutex>
#include <functional>
#include <memory>
#include <unordered_map>

namespace showcase {

struct LogEntry {
    LogLevel level;
    std::string timestamp;
    std::string message;
};

using CommandHandler = std::function<std::string(const std::string&)>;

class Console {
public:
    ~Console();

    [[nodiscard]] bool Init();
    void Render();
    void Clear();

    void AddEntry(LogLevel level, const std::string& timestamp, const std::string& message);
    void RegisterCommand(const std::string& name, CommandHandler handler);

    // State persistence
    int GetLevelFilter() const { return m_levelFilter; }
    bool GetAutoScroll() const { return m_autoScroll; }
    void SetLevelFilter(int filter) { m_levelFilter = std::clamp(filter, 0, 4); }
    void SetAutoScroll(bool scroll) { m_autoScroll = scroll; }

private:
    static ImVec4 GetLevelColor(LogLevel level);
    static const char* GetLevelLabel(LogLevel level);
    void ExecuteCommand(const std::string& input);

    std::vector<LogEntry> m_entries;
    std::mutex m_mutex;
    bool m_autoScroll = true;
    bool m_open = true;
    int m_levelFilter = 0; // index into LogLevel (0=Trace)
    ImGuiTextFilter m_textFilter;
    static constexpr size_t kMaxEntries = 2048;

    char m_inputBuf[256] = {};
    std::unordered_map<std::string, CommandHandler> m_commands;

    std::unique_ptr<LogListener> m_logListener;
};

} // namespace showcase
