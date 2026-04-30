#include "EngineRuntime.h"

EngineRuntime &EngineRuntime::GetInstance() {
    static EngineRuntime instance;
    return instance;
}

void EngineRuntime::ToggleMode() {
    SetMode(IsGameplayMode() ? EngineRuntimeMode::Editor
                             : EngineRuntimeMode::Gameplay);
}

void EngineRuntime::SetMode(EngineRuntimeMode mode) {
    settings_.mode = mode;
    settings_.showDebugUI = mode == EngineRuntimeMode::Editor;
}

EngineRuntimeMode EngineRuntime::GetMode() const { return settings_.mode; }

bool EngineRuntime::IsPlayMode() const {
    return IsGameplayMode();
}

bool EngineRuntime::IsTuningMode() const {
    return IsEditorMode();
}

bool EngineRuntime::IsGameplayMode() const {
    return settings_.mode == EngineRuntimeMode::Gameplay;
}

bool EngineRuntime::IsEditorMode() const {
    return settings_.mode == EngineRuntimeMode::Editor;
}

EngineRuntimeSettings &EngineRuntime::Settings() { return settings_; }

const EngineRuntimeSettings &EngineRuntime::Settings() const {
    return settings_;
}
