#include "panels/ViewportPanel.h"
#include "EditorContext.h"
#include "imgui.h"

void ViewportPanel::Draw(EditorContext &context) {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoCollapse;
    if (!ImGui::Begin("Viewport", nullptr, flags)) {
        position_ = ImGui::GetWindowPos();
        size_ = ImGui::GetWindowSize();
        hovered_ = ImGui::IsWindowHovered();
        focused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        context.viewportPosition = position_;
        context.viewportSize = size_;
        context.viewportHovered = hovered_;
        context.viewportFocused = focused_;
        ImGui::End();
        return;
    }

    position_ = ImGui::GetWindowPos();
    size_ = ImGui::GetWindowSize();
    hovered_ = ImGui::IsWindowHovered();
    focused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    context.viewportPosition = position_;
    context.viewportSize = size_;
    context.viewportHovered = hovered_;
    context.viewportFocused = focused_;

    const ImVec2 contentMin = ImGui::GetCursorScreenPos();
    const ImVec2 contentSize = ImGui::GetContentRegionAvail();
    const ImVec2 contentMax = {contentMin.x + contentSize.x,
                               contentMin.y + contentSize.y};

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(contentMin, contentMax,
                            IM_COL32(28, 31, 36, 235));
    drawList->AddRect(contentMin, contentMax, IM_COL32(95, 105, 118, 255));

    ImGui::Text("Mode: %s", context.gameplayMode ? "Gameplay" : "Engine");
    ImGui::Text("Viewport size: %.0f x %.0f", contentSize.x, contentSize.y);
    ImGui::Text("Hovered: %s  Focused: %s", hovered_ ? "yes" : "no",
                focused_ ? "yes" : "no");
    ImGui::Separator();
    if (context.gameplayMode) {
        ImGui::TextWrapped("Gameplay Mode: WASD move, Space jump, R reset");
    } else {
        ImGui::TextWrapped(
            "Engine Mode: WASD/QE/Arrow camera, IJKL cursor, Enter place, Backspace delete");
    }

    const char *title = "Scene Viewport";
    const char *todo = "RenderTexture display TODO";
    const ImVec2 titleSize = ImGui::CalcTextSize(title);
    const ImVec2 todoSize = ImGui::CalcTextSize(todo);
    const float centerX = contentMin.x + contentSize.x * 0.5f;
    const float centerY = contentMin.y + contentSize.y * 0.5f;
    drawList->AddText({centerX - titleSize.x * 0.5f, centerY - 16.0f},
                      IM_COL32(220, 226, 235, 255), title);
    drawList->AddText({centerX - todoSize.x * 0.5f, centerY + 6.0f},
                      IM_COL32(150, 158, 170, 255), todo);

    // TODO: Feed a RenderTexture SRV handle here and draw it with ImGui::Image.
    // TODO: Scene input can later be restricted to viewportHovered/focused.
    ImGui::End();
}
