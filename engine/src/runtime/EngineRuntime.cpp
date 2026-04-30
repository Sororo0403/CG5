#include "EngineRuntime.h"

EngineRuntime &EngineRuntime::GetInstance() {
    static EngineRuntime instance;
    return instance;
}

void EngineRuntime::ToggleMode() {
    SetMode(IsPlayMode() ? EngineRuntimeMode::Tuning : EngineRuntimeMode::Play);
}

void EngineRuntime::SetMode(EngineRuntimeMode mode) {
    settings_.mode = mode;
    settings_.showDebugUI = mode == EngineRuntimeMode::Tuning;
}

EngineRuntimeMode EngineRuntime::GetMode() const { return settings_.mode; }

bool EngineRuntime::IsPlayMode() const {
    return settings_.mode == EngineRuntimeMode::Play;
}

bool EngineRuntime::IsTuningMode() const {
    return settings_.mode == EngineRuntimeMode::Tuning;
}

EngineRuntimeSettings &EngineRuntime::Settings() { return settings_; }

const EngineRuntimeSettings &EngineRuntime::Settings() const {
    return settings_;
}
