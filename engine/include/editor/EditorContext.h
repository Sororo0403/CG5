#pragma once
#include "imgui.h"

class EditorConsole;
class IEditableScene;

struct EditorContext {
    IEditableScene *scene = nullptr;
    EditorConsole *console = nullptr;
    bool gameplayMode = false;
    bool readOnly = false;
    ImVec2 viewportPosition{0.0f, 0.0f};
    ImVec2 viewportSize{0.0f, 0.0f};
    bool viewportHovered = false;
    bool viewportFocused = false;
};
