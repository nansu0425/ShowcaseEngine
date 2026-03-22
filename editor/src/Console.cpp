#include <showcase/editor/Console.h>

#include <showcase/core/Log.h>

#include <chrono>

namespace showcase {

// ── Utilities ─────────────────────────────────────────────────────

static std::string CurrentTimestamp() {
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _MSC_VER
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    return buf;
}

// ── Console log listener ─────────────────────────────────────────

class ConsoleLogListener : public LogListener {
public:
    explicit ConsoleLogListener(Console* console) : m_console(console) {}

    void OnLogMessage(const LogMessage& msg) override {
        m_console->AddEntry(msg.level, msg.timestamp, msg.message);
    }

private:
    Console* m_console;
};

// ── Init / Shutdown ──────────────────────────────────────────────

bool Console::Init() {
    m_logListener = std::make_unique<ConsoleLogListener>(this);
    Log::AddListener(m_logListener.get());
    return true;
}

Console::~Console() {
    if (m_logListener) {
        Log::RemoveListener(m_logListener.get());
    }
}

// ── Entry management ──────────────────────────────────────────────

void Console::AddEntry(LogLevel level, const std::string& timestamp, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_entries.size() >= kMaxEntries) {
        m_entries.erase(m_entries.begin());
    }
    m_entries.push_back({level, timestamp, message});
}

void Console::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
}

// ── UI helpers ────────────────────────────────────────────────────

ImVec4 Console::GetLevelColor(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:    return {0.5f, 0.5f, 0.5f, 1.0f}; // gray
        case LogLevel::Info:     return {1.0f, 1.0f, 1.0f, 1.0f}; // white
        case LogLevel::Warn:     return {1.0f, 0.9f, 0.3f, 1.0f}; // yellow
        case LogLevel::Error:    return {1.0f, 0.3f, 0.3f, 1.0f}; // red
        case LogLevel::Critical: return {1.0f, 0.1f, 0.1f, 1.0f}; // bright red
        default:                 return {1.0f, 1.0f, 1.0f, 1.0f};
    }
}

const char* Console::GetLevelLabel(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:    return "TRACE";
        case LogLevel::Info:     return "INFO";
        case LogLevel::Warn:     return "WARN";
        case LogLevel::Error:    return "ERROR";
        case LogLevel::Critical: return "CRITICAL";
        default:                 return "???";
    }
}

// ── Commands ──────────────────────────────────────────────────────

void Console::RegisterCommand(const std::string& name, CommandHandler handler) {
    m_commands[name] = std::move(handler);
}

void Console::ExecuteCommand(const std::string& input) {
    std::string ts = CurrentTimestamp();

    // Echo command
    AddEntry(LogLevel::Trace, ts, "> " + input);

    // Must start with '/'
    if (input.empty() || input[0] != '/') {
        AddEntry(LogLevel::Warn, ts, "Commands must start with '/'");
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
            AddEntry(LogLevel::Info, ts, result);
        }
    } else {
        AddEntry(LogLevel::Warn, ts, "Unknown command: /" + command);
    }
}

// ── Rendering ────────────────────────────────────────────────────

void Console::Render() {
    if (!m_open) return;

    if (!ImGui::Begin("Console", &m_open, ImGuiWindowFlags_NoScrollbar)) {
        ImGui::End();
        return;
    }

    // Toolbar
    static const char* levelNames[] = {"Trace", "Info", "Warn", "Error", "Critical"};
    static const LogLevel levelValues[] = {
        LogLevel::Trace, LogLevel::Info, LogLevel::Warn,
        LogLevel::Error, LogLevel::Critical
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
    ImGui::BeginChild("LogScrollRegion", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()),
                      ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        LogLevel filterLevel = levelValues[m_levelFilter];

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
