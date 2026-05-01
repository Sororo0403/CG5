#include "EditorConsole.h"

void EditorConsole::AddLog(const std::string &message) {
    logs_.push_back(message);
    if (logs_.size() > 80) {
        logs_.erase(logs_.begin(), logs_.begin() + (logs_.size() - 80));
    }
}

void EditorConsole::Clear() { logs_.clear(); }
