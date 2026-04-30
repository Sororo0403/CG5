#include "panels/ConsolePanel.h"
#include "EditorConsole.h"
#include "imgui.h"
#include <string>
#include <vector>

void ConsolePanel::Draw(EditorConsole &console) {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoCollapse;
    if (!ImGui::Begin("Console / Log", nullptr, flags)) {
        ImGui::End();
        return;
    }

    const std::vector<std::string> &logs = console.GetLogs();
    if (logs.empty()) {
        ImGui::Text("Ready");
    } else {
        for (const std::string &log : logs) {
            ImGui::TextUnformatted(log.c_str());
        }
    }

    ImGui::End();
}
