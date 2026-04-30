#pragma once
#include "EditorConsole.h"
#include "EditorContext.h"
#include "panels/ConsolePanel.h"
#include "panels/HierarchyPanel.h"
#include "panels/InspectorPanel.h"
#include "panels/ViewportPanel.h"
#include <array>
#include <string>

class IEditableScene;
class Camera;
class RenderTexture;

class EditorLayer {
  public:
    void Draw(IEditableScene *scene, RenderTexture *renderTexture,
              const Camera *camera);

  private:
    void DrawToolbar(EditorContext &context);
    void DrawSceneBrowserModal(EditorContext &context);
    void DrawUnsavedChangesModal(EditorContext &context);
    void ApplyViewportInputState(const EditorContext &context);
    void RequestLoadScene(const std::string &path);
    void RequestModeSwitch();
    void ExecutePendingAction(EditorContext &context);

    enum class PendingAction {
        None,
        LoadScene,
        SwitchMode,
    };

    EditorConsole console_;
    HierarchyPanel hierarchyPanel_;
    ViewportPanel viewportPanel_;
    InspectorPanel inspectorPanel_;
    ConsolePanel consolePanel_;
    std::array<char, 128> saveAsName_{};
    std::string statusMessage_;
    std::string pendingScenePath_;
    PendingAction pendingAction_ = PendingAction::None;
    bool saveAsNameInitialized_ = false;
};
