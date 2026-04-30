#include "EditorLayer.h"
#include "EngineRuntime.h"
#include "IEditableScene.h"
#include "imgui.h"
#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace {

constexpr float kToolbarHeight = 42.0f;
constexpr float kHierarchyWidth = 260.0f;
constexpr float kInspectorWidth = 320.0f;
constexpr float kConsoleHeight = 170.0f;
constexpr const char *kLevelDirectory = "resources/levels";

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
    context.camera = camera;
    context.renderTexture = renderTexture;
    context.gameplayMode = EngineRuntime::GetInstance().IsGameplayMode();
    context.readOnly = context.gameplayMode;

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
    if (ImGui::Button(gameplayMode ? "Switch to Engine"
                                   : "Switch to Gameplay")) {
        if (dirty) {
            RequestModeSwitch();
        } else {
            runtime.SetMode(gameplayMode ? EngineRuntimeMode::Editor
                                         : EngineRuntimeMode::Gameplay);
            console_.AddLog(runtime.IsEditorMode() ? "Switched to Engine Mode"
                                                   : "Switched to Gameplay Mode");
        }
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
        if (ImGui::Button("Reset Player")) {
            if (context.scene) {
                context.scene->ResetGameplay();
            }
            console_.AddLog("Reset player to spawn");
        }
    } else {
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            std::string message;
            const bool saved =
                context.scene && context.scene->SaveScene(&message);
            statusMessage_ = saved ? "Saved successfully" : "Failed to save";
            console_.AddLog(saved ? message : "Failed to save scene: " + message);
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.0f);
        ImGui::InputText("##SaveAsName", saveAsName_.data(),
                         saveAsName_.size());
        ImGui::SameLine();
        if (ImGui::Button("Save As")) {
            std::string message;
            const std::string path =
                MakeScenePathFromInput(saveAsName_.data()).generic_string();
            const bool saved =
                context.scene && context.scene->SaveSceneAs(path, &message);
            statusMessage_ = saved ? "Saved successfully" : "Failed to save";
            console_.AddLog(saved ? message : "Failed to save scene: " + message);
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
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
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

void EditorLayer::RequestModeSwitch() {
#ifdef _DEBUG
    pendingAction_ = PendingAction::SwitchMode;
#endif // _DEBUG
}

void EditorLayer::ExecutePendingAction(EditorContext &context) {
#ifdef _DEBUG
    if (pendingAction_ == PendingAction::LoadScene) {
        std::string message;
        const bool loaded = context.scene &&
                            context.scene->LoadSceneFromPath(pendingScenePath_,
                                                             &message);
        statusMessage_ = loaded ? "Loaded successfully" : "Failed to load";
        console_.AddLog(loaded ? message : "Failed to load scene: " + message);
        saveAsNameInitialized_ = false;
        return;
    }

    if (pendingAction_ == PendingAction::SwitchMode) {
        EngineRuntime &runtime = EngineRuntime::GetInstance();
        runtime.SetMode(runtime.IsGameplayMode() ? EngineRuntimeMode::Editor
                                                 : EngineRuntimeMode::Gameplay);
        console_.AddLog(runtime.IsEditorMode() ? "Switched to Engine Mode"
                                               : "Switched to Gameplay Mode");
    }
#else
    (void)context;
#endif // _DEBUG
}
