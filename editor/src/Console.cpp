#include <showcase/editor/Console.h>
#include <showcase/core/Log.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/pattern_formatter.h>
#include <chrono>

namespace showcase {

static std::string CurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_s(&tm, &tt);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}

// Custom spdlog sink that forwards log messages to Console
template<typename Mutex>
class ConsoleSink : public spdlog::sinks::base_sink<Mutex> {
public:
    explicit ConsoleSink(Console* console) : m_console(console) {}

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
    Console* m_console;
};

void Console::Init() {
    auto sink = std::make_shared<ConsoleSink<std::mutex>>(this);
    sink->set_level(spdlog::level::trace);
    Log::AddSink(sink);
}

void Console::AddEntry(spdlog::level::level_enum level, const std::string& timestamp, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_entries.size() >= MAX_ENTRIES) {
        m_entries.erase(m_entries.begin());
    }
    m_entries.push_back({level, timestamp, message});
}

void Console::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
}

ImVec4 Console::GetLevelColor(spdlog::level::level_enum level) {
    switch (level) {
        case spdlog::level::trace:    return {0.5f, 0.5f, 0.5f, 1.0f}; // gray
        case spdlog::level::info:     return {1.0f, 1.0f, 1.0f, 1.0f}; // white
        case spdlog::level::warn:     return {1.0f, 0.9f, 0.3f, 1.0f}; // yellow
        case spdlog::level::err:      return {1.0f, 0.3f, 0.3f, 1.0f}; // red
        case spdlog::level::critical: return {1.0f, 0.1f, 0.1f, 1.0f}; // bright red
        default:                      return {1.0f, 1.0f, 1.0f, 1.0f};
    }
}

const char* Console::GetLevelLabel(spdlog::level::level_enum level) {
    switch (level) {
        case spdlog::level::trace:    return "TRACE";
        case spdlog::level::info:     return "INFO";
        case spdlog::level::warn:     return "WARN";
        case spdlog::level::err:      return "ERROR";
        case spdlog::level::critical: return "CRITICAL";
        default:                      return "???";
    }
}

void Console::RegisterCommand(const std::string& name, CommandHandler handler) {
    m_commands[name] = std::move(handler);
}

void Console::ExecuteCommand(const std::string& input) {
    auto ts = CurrentTimestamp();

    // Echo command
    AddEntry(spdlog::level::trace, ts, "> " + input);

    // Must start with '/'
    if (input.empty() || input[0] != '/') {
        AddEntry(spdlog::level::warn, ts, "Commands must start with '/'");
        return;
    }

    // Parse "/command args"
    std::string body = input.substr(1);
    size_t spacePos = body.find(' ');
    std::string command = body.substr(0, spacePos);
    std::string args = (spacePos != std::string::npos) ? body.substr(spacePos + 1) : "";

    auto it = m_commands.find(command);
    if (it != m_commands.end()) {
        std::string result = it->second(args);
        if (!result.empty()) {
            AddEntry(spdlog::level::info, ts, result);
        }
    } else {
        AddEntry(spdlog::level::warn, ts, "Unknown command: /" + command);
    }
}

void Console::Render() {
    if (!m_open) return;

    if (!ImGui::Begin("Console", &m_open, ImGuiWindowFlags_NoScrollbar)) {
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
    ImGui::BeginChild("LogScrollRegion", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

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

    // Command input
    ImGui::Separator();
    if (ImGui::InputText("##CmdInput", m_inputBuf, sizeof(m_inputBuf),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::string input(m_inputBuf);
        if (!input.empty()) {
            ExecuteCommand(input);
            m_inputBuf[0] = '\0';
        }
        ImGui::SetKeyboardFocusHere(-1);
    }

    ImGui::End();
}

} // namespace showcase
