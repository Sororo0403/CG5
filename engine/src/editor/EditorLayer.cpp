#include "EditorLayer.h"
#include "EngineRuntime.h"
#include "IEditableScene.h"
#include "imgui.h"
#include <algorithm>
#include <string>

namespace {

constexpr float kToolbarHeight = 42.0f;
constexpr float kHierarchyWidth = 260.0f;
constexpr float kInspectorWidth = 320.0f;
constexpr float kConsoleHeight = 170.0f;

} // namespace

void EditorLayer::Draw(IEditableScene *scene, RenderTexture *renderTexture) {
#ifdef _DEBUG
    EditorContext context{};
    context.scene = scene;
    context.console = &console_;
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
#else
    (void)scene;
    (void)renderTexture;
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
    ImGui::Text("Mode: %s", gameplayMode ? "Gameplay" : "Engine");

    ImGui::SameLine();
    if (ImGui::Button(gameplayMode ? "Switch to Engine"
                                   : "Switch to Gameplay")) {
        if (!gameplayMode && dirty) {
            console_.AddLog(
                "Warning: entering Gameplay with unsaved scene changes");
        }
        runtime.SetMode(gameplayMode ? EngineRuntimeMode::Editor
                                     : EngineRuntimeMode::Gameplay);
        console_.AddLog(runtime.IsEditorMode() ? "Switched to Engine Mode"
                                               : "Switched to Gameplay Mode");
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
        if (ImGui::Button("Save Scene")) {
            std::string message;
            const bool saved =
                context.scene && context.scene->SaveScene(&message);
            console_.AddLog(saved ? message : "Failed to save scene: " + message);
        }

        ImGui::SameLine();
        if (ImGui::Button("Load Scene")) {
            if (dirty) {
                console_.AddLog(
                    "Warning: loading will discard unsaved changes");
            }
            std::string message;
            const bool loaded =
                context.scene && context.scene->LoadScene(&message);
            console_.AddLog(loaded ? message : "Failed to load scene: " + message);
        }

        ImGui::SameLine();
        ImGui::Text("Scene: %s", dirty ? "Unsaved *" : "Saved");
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
