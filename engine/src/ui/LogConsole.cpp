#include <showcase/ui/LogConsole.h>
#include <showcase/core/Log.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/pattern_formatter.h>

namespace showcase {

// Custom spdlog sink that forwards log messages to LogConsole
template<typename Mutex>
class LogConsoleSink : public spdlog::sinks::base_sink<Mutex> {
public:
    explicit LogConsoleSink(LogConsole* console) : m_console(console) {}

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        // Format timestamp
        auto time = msg.time;
        auto tt = std::chrono::system_clock::to_time_t(time);
        std::tm tm{};
        localtime_s(&tm, &tt);
        char timeBuf[16];
        std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);

        // Format message payload
        std::string payload(msg.payload.data(), msg.payload.size());

        m_console->AddEntry(msg.level, timeBuf, payload);
    }

    void flush_() override {}

private:
    LogConsole* m_console;
};

void LogConsole::Init() {
    auto sink = std::make_shared<LogConsoleSink<std::mutex>>(this);
    sink->set_level(spdlog::level::trace);
    Log::AddSink(sink);
}

void LogConsole::AddEntry(spdlog::level::level_enum level, const std::string& timestamp, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_entries.size() >= MAX_ENTRIES) {
        m_entries.erase(m_entries.begin());
    }
    m_entries.push_back({level, timestamp, message});
}

void LogConsole::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
}

ImVec4 LogConsole::GetLevelColor(spdlog::level::level_enum level) {
    switch (level) {
        case spdlog::level::trace:    return {0.5f, 0.5f, 0.5f, 1.0f}; // gray
        case spdlog::level::info:     return {1.0f, 1.0f, 1.0f, 1.0f}; // white
        case spdlog::level::warn:     return {1.0f, 0.9f, 0.3f, 1.0f}; // yellow
        case spdlog::level::err:      return {1.0f, 0.3f, 0.3f, 1.0f}; // red
        case spdlog::level::critical: return {1.0f, 0.1f, 0.1f, 1.0f}; // bright red
        default:                      return {1.0f, 1.0f, 1.0f, 1.0f};
    }
}

const char* LogConsole::GetLevelLabel(spdlog::level::level_enum level) {
    switch (level) {
        case spdlog::level::trace:    return "TRACE";
        case spdlog::level::info:     return "INFO";
        case spdlog::level::warn:     return "WARN";
        case spdlog::level::err:      return "ERROR";
        case spdlog::level::critical: return "CRITICAL";
        default:                      return "???";
    }
}

void LogConsole::Render() {
    if (!m_open) return;

    if (!ImGui::Begin("Log Console", &m_open)) {
        ImGui::End();
        return;
    }

    // Toolbar
    static const char* levelNames[] = {"Trace", "Info", "Warn", "Error", "Critical"};
    static const spdlog::level::level_enum levelValues[] = {
        spdlog::level::trace, spdlog::level::info, spdlog::level::warn,
        spdlog::level::err, spdlog::level::critical
    };

    ImGui::SetNextItemWidth(100.0f);
    ImGui::Combo("##Level", &m_levelFilter, levelNames, IM_ARRAYSIZE(levelNames));
    ImGui::SameLine();
    m_textFilter.Draw("##Filter", 180.0f);
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        Clear();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &m_autoScroll);
    ImGui::Separator();

    // Log entries
    ImGui::BeginChild("LogScrollRegion", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        spdlog::level::level_enum filterLevel = levelValues[m_levelFilter];

        for (const auto& entry : m_entries) {
            if (entry.level < filterLevel) continue;

            // Build display string for text filter
            char lineBuf[512];
            std::snprintf(lineBuf, sizeof(lineBuf), "[%s] [%s] %s",
                entry.timestamp.c_str(), GetLevelLabel(entry.level), entry.message.c_str());

            if (!m_textFilter.PassFilter(lineBuf)) continue;

            ImGui::PushStyleColor(ImGuiCol_Text, GetLevelColor(entry.level));
            ImGui::TextUnformatted(lineBuf);
            ImGui::PopStyleColor();
        }
    }

    if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
    ImGui::End();
}

} // namespace showcase
