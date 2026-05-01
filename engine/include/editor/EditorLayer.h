#pragma once
#include "EditorCommand.h"
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
    void DrawOverwriteSceneModal(EditorContext &context);
    void ApplyViewportInputState(const EditorContext &context);
    void RequestNewScene(EditorContext &context);
    void RequestLoadScene(const std::string &path);
    void RequestSaveSceneAs(EditorContext &context);
    void ExecuteSaveSceneAs(EditorContext &context,
                            const std::string &path);
    void StartPlay(EditorContext &context);
    void StopPlay(EditorContext &context);
    void ExecutePendingAction(EditorContext &context);
    void HandleUndoRedoShortcuts();

    enum class PendingAction {
        None,
        NewScene,
        LoadScene,
    };

    EditorConsole console_;
    EditorCommandManager commandManager_;
    HierarchyPanel hierarchyPanel_;
    ViewportPanel viewportPanel_;
    InspectorPanel inspectorPanel_;
    ConsolePanel consolePanel_;
    std::array<char, 128> saveAsName_{};
    std::string statusMessage_;
    std::string pendingScenePath_;
    std::string pendingSaveAsPath_;
    std::string playStartSceneState_;
    PendingAction pendingAction_ = PendingAction::None;
    bool playSessionActive_ = false;
    bool playStartDirty_ = false;
    bool saveAsNameInitialized_ = false;
};
