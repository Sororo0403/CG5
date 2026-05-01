#pragma once
#include "EditorCommand.h"
#include "EditorConsole.h"
#include "EditorContext.h"
#include "panels/ConsolePanel.h"
#include "panels/HierarchyPanel.h"
#include "panels/InspectorPanel.h"
#include "panels/ViewportPanel.h"
#include <array>
#include <chrono>
#include <string>
#include <vector>

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
    void UpdateAutosave(EditorContext &context);
    bool TryAutosave(EditorContext &context);
    std::string MakeAutosavePath(const IEditableScene &scene) const;
    void EnsureRecentScenesLoaded();
    void LoadRecentScenes();
    void SaveRecentScenes() const;
    void AddRecentScene(const std::string &path);
    void DrawRecentScenesPopup(EditorContext &context);
    void LoadSceneWithDirtyCheck(EditorContext &context,
                                 const std::string &path);
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
    std::string autosaveStatusMessage_;
    std::string pendingScenePath_;
    std::string pendingSaveAsPath_;
    std::string playStartSceneState_;
    std::vector<std::string> recentScenes_;
    std::chrono::steady_clock::time_point lastAutosaveTime_{};
    PendingAction pendingAction_ = PendingAction::None;
    bool playSessionActive_ = false;
    bool playStartDirty_ = false;
    bool saveAsNameInitialized_ = false;
    bool recentScenesLoaded_ = false;
    bool autosaveClockInitialized_ = false;
};
