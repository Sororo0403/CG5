#include "EditorLayer.h"
#include "EngineRuntime.h"
#include "IEditableScene.h"
#include "imgui.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr float kToolbarHeight = 42.0f;
constexpr float kHierarchyWidth = 260.0f;
constexpr float kInspectorWidth = 320.0f;
constexpr float kConsoleHeight = 170.0f;
constexpr const char *kLevelDirectory = "resources/levels";

bool HasSceneFilenameInput(const char *input) {
    if (!input) {
        return false;
    }
    for (const char *cursor = input; *cursor != '\0'; ++cursor) {
        if (!std::isspace(static_cast<unsigned char>(*cursor))) {
            return true;
        }
    }
    return false;
}

std::filesystem::path MakeScenePathFromInput(const char *input) {
    std::filesystem::path filename(input ? input : "");
    filename = filename.filename();
    if (filename.empty()) {
        filename = "untitled.json";
    }
    filename.replace_extension(".json");
    return std::filesystem::path(kLevelDirectory) / filename;
}

std::vector<std::filesystem::path> EnumerateSceneFiles() {
    std::vector<std::filesystem::path> files;
    std::error_code error;
    if (!std::filesystem::exists(kLevelDirectory, error)) {
        return files;
    }

    for (const std::filesystem::directory_entry &entry :
         std::filesystem::directory_iterator(kLevelDirectory, error)) {
        if (error) {
            break;
        }
        if (!entry.is_regular_file(error)) {
            continue;
        }
        const std::filesystem::path path = entry.path();
        if (path.extension() == ".json") {
            files.push_back(path);
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}

} // namespace

void EditorLayer::Draw(IEditableScene *scene, RenderTexture *renderTexture,
                       const Camera *camera) {
#ifdef _DEBUG
    EditorContext context{};
    context.scene = scene;
    context.console = &console_;
    context.commands = &commandManager_;
    context.camera = camera;
    context.renderTexture = renderTexture;
    context.gameplayMode = EngineRuntime::GetInstance().IsGameplayMode();
    context.readOnly = context.gameplayMode;
    HandleUndoRedoShortcuts();

    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    const ImVec2 origin = viewport->WorkPos;
    const ImVec2 size = viewport->WorkSize;

    ImGui::SetNextWindowPos(origin);
    ImGui::SetNextWindowSize({size.x, kToolbarHeight});
    DrawToolbar(context);

    ImGui::SetNextWindowPos({origin.x, origin.y + kToolbarHeight});
    ImGui::SetNextWindowSize(
        {kHierarchyWidth, (std::max)(0.0f, size.y - kToolbarHeight - kConsoleHeight)});
    hierarchyPanel_.Draw(context);

    ImGui::SetNextWindowPos(
        {origin.x + kHierarchyWidth, origin.y + kToolbarHeight});
    ImGui::SetNextWindowSize(
        {(std::max)(0.0f, size.x - kHierarchyWidth - kInspectorWidth),
         (std::max)(0.0f, size.y - kToolbarHeight - kConsoleHeight)});
    viewportPanel_.Draw(context);
    ApplyViewportInputState(context);

    ImGui::SetNextWindowPos(
        {origin.x + size.x - kInspectorWidth, origin.y + kToolbarHeight});
    ImGui::SetNextWindowSize(
        {kInspectorWidth, (std::max)(0.0f, size.y - kToolbarHeight - kConsoleHeight)});
    inspectorPanel_.Draw(context);

    ImGui::SetNextWindowPos({origin.x, origin.y + size.y - kConsoleHeight});
    ImGui::SetNextWindowSize({size.x, kConsoleHeight});
    consolePanel_.Draw(console_);

    DrawSceneBrowserModal(context);
    DrawUnsavedChangesModal(context);
    DrawOverwriteSceneModal(context);
#else
    (void)scene;
    (void)renderTexture;
    (void)camera;
#endif // _DEBUG
}

void EditorLayer::ApplyViewportInputState(const EditorContext &context) {
#ifdef _DEBUG
    EngineRuntimeSettings &settings = EngineRuntime::GetInstance().Settings();
    settings.viewportInputEnabled =
        context.viewportHovered || context.viewportFocused;
    settings.viewportX = context.viewportImagePosition.x;
    settings.viewportY = context.viewportImagePosition.y;
    settings.viewportWidth = context.viewportImageSize.x;
    settings.viewportHeight = context.viewportImageSize.y;
    settings.viewportMouseX = context.viewportMousePosition.x;
    settings.viewportMouseY = context.viewportMousePosition.y;
    settings.viewportClicked = context.viewportClicked;
    settings.viewportGizmoUsing = context.viewportGizmoUsing;
#else
    (void)context;
#endif // _DEBUG
}

void EditorLayer::DrawToolbar(EditorContext &context) {
#ifdef _DEBUG
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoTitleBar;
    if (!ImGui::Begin("Editor Toolbar", nullptr, flags)) {
        ImGui::End();
        return;
    }

    EngineRuntime &runtime = EngineRuntime::GetInstance();
    const bool gameplayMode = runtime.IsGameplayMode();
    const bool dirty = context.scene && context.scene->IsSceneDirty();
    const std::string sceneName =
        context.scene ? context.scene->GetCurrentSceneName() : std::string{};

    if (!saveAsNameInitialized_ && context.scene) {
        const std::string filename =
            sceneName.empty() ? "game_stage.json" : sceneName + ".json";
        saveAsName_.fill('\0');
        const size_t copyLength =
            (std::min)(filename.size(), saveAsName_.size() - 1);
        std::copy_n(filename.data(), copyLength, saveAsName_.data());
        saveAsNameInitialized_ = true;
    }

    ImGui::Text("Mode: %s", gameplayMode ? "Gameplay" : "Engine");

    ImGui::SameLine();
    const bool undoDisabled = gameplayMode || !commandManager_.CanUndo();
    if (undoDisabled) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Undo")) {
        commandManager_.Undo();
        statusMessage_ = "Undo";
        console_.AddLog("Undo");
    }
    if (undoDisabled) {
        ImGui::EndDisabled();
    }

    ImGui::SameLine();
    const bool redoDisabled = gameplayMode || !commandManager_.CanRedo();
    if (redoDisabled) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Redo")) {
        commandManager_.Redo();
        statusMessage_ = "Redo";
        console_.AddLog("Redo");
    }
    if (redoDisabled) {
        ImGui::EndDisabled();
    }

    if (gameplayMode) {
        const bool paused =
            context.scene ? context.scene->IsGameplayPaused() : false;
        ImGui::SameLine();
        if (ImGui::Button(paused ? "Resume" : "Pause")) {
            if (context.scene) {
                context.scene->SetGameplayPaused(!paused);
            }
            console_.AddLog(paused ? "Gameplay resumed" : "Gameplay paused");
        }

        ImGui::SameLine();
        if (ImGui::Button("Stop")) {
            StopPlay(context);
        }

        ImGui::SameLine();
        if (ImGui::Button("Reset Player")) {
            if (context.scene) {
                context.scene->ResetGameplay();
            }
            console_.AddLog("Reset player to spawn");
        }
    } else {
        ImGui::SameLine();
        if (ImGui::Button("New")) {
            RequestNewScene(context);
        }

        ImGui::SameLine();
        if (ImGui::Button("Play")) {
            StartPlay(context);
        }

        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            if (context.scene && context.scene->HasScenePath()) {
                std::string message;
                const bool saved = context.scene->SaveScene(&message);
                statusMessage_ =
                    saved ? "Saved successfully" : "Failed to save";
                console_.AddLog(saved ? message
                                      : "Failed to save scene: " + message);
                if (saved) {
                    commandManager_.Clear();
                }
            } else {
                RequestSaveSceneAs(context);
            }
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputText("##SaveAsName", saveAsName_.data(),
                         saveAsName_.size());
        ImGui::SameLine();
        if (ImGui::Button("Save As")) {
            RequestSaveSceneAs(context);
        }

        ImGui::SameLine();
        if (ImGui::Button("Load...")) {
            ImGui::OpenPopup("Scene Browser");
        }

        ImGui::SameLine();
        ImGui::Text("Scene: %s [%s]", sceneName.empty() ? "None" : sceneName.c_str(),
                    dirty ? "Unsaved" : "Saved");

        if (!statusMessage_.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", statusMessage_.c_str());
        }

        ImGui::SameLine();
        ImGui::TextDisabled("Recent/Autosave: TODO");
    }

    ImGui::SameLine();
    const size_t objectCount =
        context.scene ? context.scene->GetEditableObjectCount() : 0;
    ImGui::Text("Objects: %zu", objectCount);
    ImGui::End();
#else
    (void)context;
#endif // _DEBUG
}

void EditorLayer::DrawSceneBrowserModal(EditorContext &context) {
#ifdef _DEBUG
    if (!ImGui::BeginPopupModal("Scene Browser", nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::Text("Load Scene");
    ImGui::Separator();
    const std::vector<std::filesystem::path> files = EnumerateSceneFiles();
    if (files.empty()) {
        ImGui::TextDisabled("No scene files found in %s", kLevelDirectory);
    } else {
        const float listHeight = 220.0f;
        if (ImGui::BeginChild("SceneFileList", ImVec2(420.0f, listHeight),
                              true)) {
            const std::string currentPath =
                context.scene ? context.scene->GetCurrentScenePath() : "";
            for (const std::filesystem::path &path : files) {
                const std::string genericPath = path.generic_string();
                const bool selected = genericPath == currentPath;
                if (ImGui::Selectable(path.filename().string().c_str(),
                                      selected)) {
                    if (context.scene && context.scene->IsSceneDirty()) {
                        RequestLoadScene(genericPath);
                    } else {
                        pendingScenePath_ = genericPath;
                        pendingAction_ = PendingAction::LoadScene;
                        ExecutePendingAction(context);
                        pendingAction_ = PendingAction::None;
                        pendingScenePath_.clear();
                    }
                    ImGui::CloseCurrentPopup();
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", genericPath.c_str());
                }
            }
        }
        ImGui::EndChild();
    }

    ImGui::Separator();
    if (ImGui::Button("Close")) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
#else
    (void)context;
#endif // _DEBUG
}

void EditorLayer::DrawUnsavedChangesModal(EditorContext &context) {
#ifdef _DEBUG
    if (pendingAction_ != PendingAction::None) {
        ImGui::OpenPopup("Unsaved Changes");
    }
    if (!ImGui::BeginPopupModal("Unsaved Changes", nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::Text("There are unsaved changes. Continue?");
    ImGui::Spacing();
    if (ImGui::Button("Yes")) {
        ExecutePendingAction(context);
        pendingAction_ = PendingAction::None;
        pendingScenePath_.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("No")) {
        pendingAction_ = PendingAction::None;
        pendingScenePath_.clear();
        statusMessage_ = "Canceled";
        console_.AddLog("Canceled");
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
#else
    (void)context;
#endif // _DEBUG
}

void EditorLayer::DrawOverwriteSceneModal(EditorContext &context) {
#ifdef _DEBUG
    if (!pendingSaveAsPath_.empty()) {
        ImGui::OpenPopup("Overwrite Scene");
    }
    if (!ImGui::BeginPopupModal("Overwrite Scene", nullptr,
                                ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::Text("Scene file already exists. Overwrite it?");
    ImGui::TextDisabled("%s", pendingSaveAsPath_.c_str());
    ImGui::Spacing();
    if (ImGui::Button("Overwrite")) {
        console_.AddLog("Overwriting existing scene: " + pendingSaveAsPath_);
        ExecuteSaveSceneAs(context, pendingSaveAsPath_);
        pendingSaveAsPath_.clear();
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        pendingSaveAsPath_.clear();
        statusMessage_ = "Canceled";
        console_.AddLog("Canceled");
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
#else
    (void)context;
#endif // _DEBUG
}

void EditorLayer::RequestNewScene(EditorContext &context) {
#ifdef _DEBUG
    if (context.scene && context.scene->IsSceneDirty()) {
        pendingAction_ = PendingAction::NewScene;
        return;
    }
    pendingAction_ = PendingAction::NewScene;
    ExecutePendingAction(context);
    pendingAction_ = PendingAction::None;
#else
    (void)context;
#endif // _DEBUG
}

void EditorLayer::RequestLoadScene(const std::string &path) {
#ifdef _DEBUG
    pendingScenePath_ = path;
    pendingAction_ = PendingAction::LoadScene;
#else
    (void)path;
#endif // _DEBUG
}

void EditorLayer::RequestSaveSceneAs(EditorContext &context) {
#ifdef _DEBUG
    if (!context.scene) {
        statusMessage_ = "Failed to save";
        console_.AddLog("Failed to save scene: no editable scene");
        return;
    }
    if (!HasSceneFilenameInput(saveAsName_.data())) {
        statusMessage_ = "Failed to save";
        console_.AddLog("Failed to save scene: file name is empty");
        return;
    }

    const std::string path =
        MakeScenePathFromInput(saveAsName_.data()).generic_string();
    if (std::filesystem::exists(path)) {
        pendingSaveAsPath_ = path;
        return;
    }
    ExecuteSaveSceneAs(context, path);
#else
    (void)context;
#endif // _DEBUG
}

void EditorLayer::ExecuteSaveSceneAs(EditorContext &context,
                                     const std::string &path) {
#ifdef _DEBUG
    std::string message;
    const bool saved =
        context.scene && context.scene->SaveSceneAs(path, &message);
    statusMessage_ = saved ? "Saved successfully" : "Failed to save";
    console_.AddLog(saved ? message : "Failed to save scene: " + message);
    if (saved) {
        saveAsNameInitialized_ = false;
        commandManager_.Clear();
    }
#else
    (void)context;
    (void)path;
#endif // _DEBUG
}

void EditorLayer::StartPlay(EditorContext &context) {
#ifdef _DEBUG
    if (!context.scene) {
        console_.AddLog("Failed to start play: no editable scene");
        return;
    }

    std::string state;
    std::string message;
    if (!context.scene->CaptureSceneState(&state, &message)) {
        console_.AddLog("Failed to capture editor scene state: " + message);
        return;
    }

    playStartSceneState_ = std::move(state);
    playStartDirty_ = context.scene->IsSceneDirty();
    playSessionActive_ = true;
    context.scene->SetGameplayPaused(false);
    EngineRuntime::GetInstance().SetMode(EngineRuntimeMode::Gameplay);
    statusMessage_ = "Play";
    console_.AddLog("Play started");
#else
    (void)context;
#endif // _DEBUG
}

void EditorLayer::StopPlay(EditorContext &context) {
#ifdef _DEBUG
    EngineRuntime::GetInstance().SetMode(EngineRuntimeMode::Editor);

    bool restored = true;
    if (context.scene && playSessionActive_ && !playStartSceneState_.empty()) {
        std::string message;
        restored = context.scene->RestoreSceneState(playStartSceneState_, &message);
        if (!restored) {
            console_.AddLog("Failed to restore editor scene state: " + message);
        } else if (playStartDirty_) {
            context.scene->MarkSceneDirty();
        } else {
            context.scene->ClearSceneDirty();
        }
    } else if (context.scene) {
        context.scene->ClearHoveredGridCell();
    }

    if (context.scene) {
        context.scene->SetGameplayPaused(false);
    }
    playSessionActive_ = false;
    playStartDirty_ = false;
    playStartSceneState_.clear();
    statusMessage_ = restored ? "Stopped" : "Stop restore failed";
    console_.AddLog("Play stopped");
#else
    (void)context;
#endif // _DEBUG
}

void EditorLayer::ExecutePendingAction(EditorContext &context) {
#ifdef _DEBUG
    if (pendingAction_ == PendingAction::NewScene) {
        std::string message;
        const bool created = context.scene && context.scene->NewScene(&message);
        statusMessage_ =
            created ? "New scene created" : "Failed to create scene";
        console_.AddLog(created ? message
                                : "Failed to create scene: " + message);
        saveAsNameInitialized_ = false;
        if (created) {
            commandManager_.Clear();
        }
        return;
    }

    if (pendingAction_ == PendingAction::LoadScene) {
        std::string message;
        const bool loaded = context.scene &&
                            context.scene->LoadSceneFromPath(pendingScenePath_,
                                                             &message);
        statusMessage_ = loaded ? "Loaded successfully" : "Failed to load";
        console_.AddLog(loaded ? message : "Failed to load scene: " + message);
        saveAsNameInitialized_ = false;
        if (loaded) {
            commandManager_.Clear();
        }
        return;
    }
#else
    (void)context;
#endif // _DEBUG
}

void EditorLayer::HandleUndoRedoShortcuts() {
#ifdef _DEBUG
    ImGuiIO &io = ImGui::GetIO();
    if (io.WantTextInput || EngineRuntime::GetInstance().IsGameplayMode()) {
        return;
    }

    const bool ctrl = io.KeyCtrl;
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        commandManager_.Undo();
        statusMessage_ = "Undo";
        console_.AddLog("Undo");
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
        commandManager_.Redo();
        statusMessage_ = "Redo";
        console_.AddLog("Redo");
    }
#endif // _DEBUG
}
