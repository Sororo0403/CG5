#pragma once
#include "imgui.h"

class EditorConsole;
class EditorCommandManager;
class IEditableScene;
class Camera;
class RenderTexture;

struct EditorContext {
    IEditableScene *scene = nullptr;
    EditorConsole *console = nullptr;
    EditorCommandManager *commands = nullptr;
    const Camera *camera = nullptr;
    RenderTexture *renderTexture = nullptr;
    bool gameplayMode = false;
    bool readOnly = false;
    ImVec2 viewportPosition{0.0f, 0.0f};
    ImVec2 viewportSize{0.0f, 0.0f};
    bool viewportHovered = false;
    bool viewportFocused = false;
    ImVec2 viewportImagePosition{0.0f, 0.0f};
    ImVec2 viewportImageSize{0.0f, 0.0f};
    ImVec2 viewportMousePosition{0.0f, 0.0f};
    bool viewportImageHovered = false;
    bool viewportClicked = false;
    bool viewportGizmoUsing = false;
};
