#include "EditorLayer.h"
#include "EngineRuntime.h"
#include "IEditableScene.h"
#include "imgui.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <ctime>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr float kToolbarHeight = 42.0f;
constexpr float kHierarchyWidth = 260.0f;
constexpr float kInspectorWidth = 320.0f;
constexpr float kConsoleHeight = 170.0f;
constexpr const char *kLevelDirectory = "resources/levels";
constexpr const char *kEditorDirectory = "resources/editor";
constexpr const char *kAutosaveDirectory = "resources/editor/autosaves";
constexpr const char *kRecentScenesPath = "resources/editor/recent_scenes.json";
constexpr int kMaxRecentScenes = 8;
constexpr auto kAutosaveInterval = std::chrono::seconds(60);

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

std::string MakeSafeFilenameStem(const std::string &name) {
    std::string safe;
    safe.reserve(name.size());
    for (const char ch : name) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isalnum(uch) || ch == '_' || ch == '-' || ch == '.') {
            safe.push_back(ch);
        } else {
            safe.push_back('_');
        }
    }
    if (safe.empty() || safe == "." || safe == "..") {
        return "untitled";
    }
    return safe;
}

std::string FormatClockTime(std::chrono::system_clock::time_point time) {
    const std::time_t rawTime = std::chrono::system_clock::to_time_t(time);
    std::tm localTime{};
#if defined(_WIN32)
    localtime_s(&localTime, &rawTime);
#else
    localtime_r(&rawTime, &localTime);
#endif
    char buffer[16]{};
    std::strftime(buffer, sizeof(buffer), "%H:%M:%S", &localTime);
    return buffer;
}

bool IsAutosaveSceneFile(const std::filesystem::path &path) {
    const std::string filename = path.filename().string();
    return filename.find(".autosave.") != std::string::npos;
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
        if (path.extension() == ".json" && !IsAutosaveSceneFile(path)) {
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
    EnsureRecentScenesLoaded();
    UpdateAutosave(context);

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
                    AddRecentScene(context.scene->GetCurrentScenePath());
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
        if (ImGui::Button("Recent")) {
            ImGui::OpenPopup("Recent Scenes");
        }
        DrawRecentScenesPopup(context);

        ImGui::SameLine();
        ImGui::Text("Scene: %s [%s]", sceneName.empty() ? "None" : sceneName.c_str(),
                    dirty ? "Unsaved" : "Saved");

        if (!statusMessage_.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", statusMessage_.c_str());
        }

        ImGui::SameLine();
        ImGui::TextDisabled("%s",
                            autosaveStatusMessage_.empty()
                                ? "Autosave: waiting"
                                : autosaveStatusMessage_.c_str());
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
                    LoadSceneWithDirtyCheck(context, genericPath);
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

void EditorLayer::UpdateAutosave(EditorContext &context) {
#ifdef _DEBUG
    const auto now = std::chrono::steady_clock::now();
    if (!autosaveClockInitialized_) {
        lastAutosaveTime_ = now;
        autosaveClockInitialized_ = true;
    }

    if (context.gameplayMode || !context.scene ||
        !context.scene->IsSceneDirty()) {
        lastAutosaveTime_ = now;
        return;
    }

    if (now - lastAutosaveTime_ < kAutosaveInterval) {
        return;
    }

    lastAutosaveTime_ = now;
    TryAutosave(context);
#else
    (void)context;
#endif // _DEBUG
}

bool EditorLayer::TryAutosave(EditorContext &context) {
#ifdef _DEBUG
    if (!context.scene) {
        return false;
    }

    std::string message;
    const std::string path = MakeAutosavePath(*context.scene);
    const bool saved = context.scene->SaveSceneSnapshot(path, &message);
    if (!saved) {
        console_.AddLog("Autosave failed: " + message);
        return false;
    }

    autosaveStatusMessage_ =
        "Autosaved " + FormatClockTime(std::chrono::system_clock::now());
    return true;
#else
    (void)context;
    return false;
#endif // _DEBUG
}

std::string EditorLayer::MakeAutosavePath(const IEditableScene &scene) const {
#ifdef _DEBUG
    const std::string stem = MakeSafeFilenameStem(scene.GetCurrentSceneName());
    return (std::filesystem::path(kAutosaveDirectory) /
            (stem + ".autosave.json"))
        .generic_string();
#else
    (void)scene;
    return {};
#endif // _DEBUG
}

void EditorLayer::EnsureRecentScenesLoaded() {
#ifdef _DEBUG
    if (recentScenesLoaded_) {
        return;
    }
    LoadRecentScenes();
    recentScenesLoaded_ = true;
#endif // _DEBUG
}

void EditorLayer::LoadRecentScenes() {
#ifdef _DEBUG
    recentScenes_.clear();

    std::ifstream file(kRecentScenesPath);
    if (!file) {
        return;
    }

    try {
        nlohmann::json root;
        file >> root;
        const nlohmann::json *array = nullptr;
        if (root.is_object() && root.contains("recentScenes") &&
            root["recentScenes"].is_array()) {
            array = &root["recentScenes"];
        } else if (root.is_array()) {
            array = &root;
        }
        if (!array) {
            return;
        }

        for (const nlohmann::json &entry : *array) {
            if (!entry.is_string()) {
                continue;
            }
            const std::string normalized =
                std::filesystem::path(entry.get<std::string>())
                    .generic_string();
            if (normalized.empty() || IsAutosaveSceneFile(normalized)) {
                continue;
            }
            if (std::find(recentScenes_.begin(), recentScenes_.end(),
                          normalized) != recentScenes_.end()) {
                continue;
            }
            recentScenes_.push_back(normalized);
            if (recentScenes_.size() >= kMaxRecentScenes) {
                break;
            }
        }
    } catch (const nlohmann::json::exception &) {
        recentScenes_.clear();
    }
#endif // _DEBUG
}

void EditorLayer::SaveRecentScenes() const {
#ifdef _DEBUG
    std::error_code error;
    std::filesystem::create_directories(kEditorDirectory, error);
    if (error) {
        return;
    }

    nlohmann::json root;
    root["recentScenes"] = recentScenes_;
    std::ofstream file(kRecentScenesPath);
    if (file) {
        file << root.dump(2);
    }
#endif // _DEBUG
}

void EditorLayer::AddRecentScene(const std::string &path) {
#ifdef _DEBUG
    if (path.empty()) {
        return;
    }

    const std::string normalized = std::filesystem::path(path).generic_string();
    if (IsAutosaveSceneFile(normalized)) {
        return;
    }

    recentScenes_.erase(
        std::remove(recentScenes_.begin(), recentScenes_.end(), normalized),
        recentScenes_.end());
    recentScenes_.insert(recentScenes_.begin(), normalized);
    if (recentScenes_.size() > kMaxRecentScenes) {
        recentScenes_.resize(kMaxRecentScenes);
    }
    if (recentScenesLoaded_) {
        SaveRecentScenes();
    }
#else
    (void)path;
#endif // _DEBUG
}

void EditorLayer::DrawRecentScenesPopup(EditorContext &context) {
#ifdef _DEBUG
    if (!ImGui::BeginPopup("Recent Scenes")) {
        return;
    }

    if (recentScenes_.empty()) {
        ImGui::TextDisabled("No recent scenes");
    } else {
        for (const std::string &path : recentScenes_) {
            const bool exists = std::filesystem::exists(path);
            const std::string label =
                std::filesystem::path(path).filename().string() +
                (exists ? "" : " (missing)");
            if (!exists) {
                ImGui::BeginDisabled();
            }
            if (ImGui::Selectable(label.c_str(), false) && exists) {
                LoadSceneWithDirtyCheck(context, path);
                ImGui::CloseCurrentPopup();
            }
            if (!exists) {
                ImGui::EndDisabled();
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", path.c_str());
            }
        }
    }

    ImGui::EndPopup();
#else
    (void)context;
#endif // _DEBUG
}

void EditorLayer::LoadSceneWithDirtyCheck(EditorContext &context,
                                          const std::string &path) {
#ifdef _DEBUG
    if (context.scene && context.scene->IsSceneDirty()) {
        RequestLoadScene(path);
        return;
    }

    pendingScenePath_ = path;
    pendingAction_ = PendingAction::LoadScene;
    ExecutePendingAction(context);
    pendingAction_ = PendingAction::None;
    pendingScenePath_.clear();
#else
    (void)context;
    (void)path;
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
        AddRecentScene(path);
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
            if (context.scene && context.scene->HasScenePath()) {
                AddRecentScene(context.scene->GetCurrentScenePath());
            }
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
            AddRecentScene(pendingScenePath_);
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
