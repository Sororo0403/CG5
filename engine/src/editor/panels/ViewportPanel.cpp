#include "panels/ViewportPanel.h"
#include "EditorContext.h"
#include "RenderTexture.h"
#include "imgui.h"
#include <algorithm>

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

    context.viewportImagePosition = contentMin;
    context.viewportImageSize = {0.0f, 0.0f};
    context.viewportMousePosition = {0.0f, 0.0f};
    context.viewportClicked = false;

    RenderTexture *renderTexture = context.renderTexture;
    if (!renderTexture || renderTexture->GetWidth() <= 0 ||
        renderTexture->GetHeight() <= 0 || contentSize.x <= 0.0f ||
        contentSize.y <= 0.0f) {
        ImGui::End();
        return;
    }

    const float textureAspect =
        static_cast<float>(renderTexture->GetWidth()) /
        static_cast<float>(renderTexture->GetHeight());
    ImVec2 imageSize = contentSize;
    if (textureAspect > 0.0f) {
        const float contentAspect = contentSize.x / contentSize.y;
        if (contentAspect > textureAspect) {
            imageSize.x = contentSize.y * textureAspect;
        } else {
            imageSize.y = contentSize.x / textureAspect;
        }
    }

    imageSize.x = (std::max)(1.0f, imageSize.x);
    imageSize.y = (std::max)(1.0f, imageSize.y);

    const ImVec2 imagePos = {
        contentMin.x + (contentSize.x - imageSize.x) * 0.5f,
        contentMin.y + (contentSize.y - imageSize.y) * 0.5f};

    context.viewportPosition = imagePos;
    context.viewportSize = imageSize;
    context.viewportImagePosition = imagePos;
    context.viewportImageSize = imageSize;

    ImGui::SetCursorScreenPos(imagePos);
    const D3D12_GPU_DESCRIPTOR_HANDLE textureHandle =
        renderTexture->GetGpuHandle();
    ImGui::Image(static_cast<ImTextureID>(textureHandle.ptr), imageSize);

    hovered_ = hovered_ || ImGui::IsItemHovered();
    context.viewportHovered = hovered_;

    const ImVec2 mousePos = ImGui::GetMousePos();
    context.viewportMousePosition = {mousePos.x - imagePos.x,
                                     mousePos.y - imagePos.y};
    context.viewportClicked =
        ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

    // TODO: Use viewportMousePosition with camera projection data to build
    // screen-to-world picking for object selection and placement.
    ImGui::End();
}
