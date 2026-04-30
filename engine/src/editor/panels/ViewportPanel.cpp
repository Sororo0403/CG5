#include "panels/ViewportPanel.h"
#include "Camera.h"
#include "EditorCommand.h"
#include "EditorConsole.h"
#include "EngineRuntime.h"
#include "EditorContext.h"
#include "IEditableObject.h"
#include "IEditableScene.h"
#include "RenderTexture.h"
#include "ImGuizmo/ImGuizmo.h"
#include "imgui.h"
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>

namespace {

constexpr float kMaxPosition = 10000.0f;
constexpr float kMaxScale = 10000.0f;

DirectX::XMFLOAT4X4 TransformToMatrix(const EditableTransform &transform) {
    DirectX::XMVECTOR rotation =
        DirectX::XMQuaternionNormalize(DirectX::XMLoadFloat4(&transform.rotation));
    DirectX::XMMATRIX matrix =
        DirectX::XMMatrixScaling(transform.scale.x, transform.scale.y,
                                 transform.scale.z) *
        DirectX::XMMatrixRotationQuaternion(rotation) *
        DirectX::XMMatrixTranslation(transform.position.x,
                                     transform.position.y,
                                     transform.position.z);

    DirectX::XMFLOAT4X4 result{};
    DirectX::XMStoreFloat4x4(&result, matrix);
    return result;
}

float SanitizeFloat(float value, float minValue, float maxValue,
                    float fallback) {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return (std::clamp)(value, minValue, maxValue);
}

EditableTransform MatrixToTransform(const DirectX::XMFLOAT4X4 &matrix,
                                    const EditableTransform &fallback) {
    DirectX::XMVECTOR scale{};
    DirectX::XMVECTOR rotation{};
    DirectX::XMVECTOR translation{};
    if (!DirectX::XMMatrixDecompose(
            &scale, &rotation, &translation, DirectX::XMLoadFloat4x4(&matrix))) {
        return fallback;
    }

    EditableTransform result = fallback;
    DirectX::XMStoreFloat3(&result.position, translation);
    DirectX::XMStoreFloat4(
        &result.rotation, DirectX::XMQuaternionNormalize(rotation));
    DirectX::XMStoreFloat3(&result.scale, scale);

    result.position.x =
        SanitizeFloat(result.position.x, -kMaxPosition, kMaxPosition,
                      fallback.position.x);
    result.position.y =
        SanitizeFloat(result.position.y, -kMaxPosition, kMaxPosition,
                      fallback.position.y);
    result.position.z =
        SanitizeFloat(result.position.z, -kMaxPosition, kMaxPosition,
                      fallback.position.z);
    result.scale.x =
        SanitizeFloat(result.scale.x, 0.01f, kMaxScale, fallback.scale.x);
    result.scale.y =
        SanitizeFloat(result.scale.y, 0.01f, kMaxScale, fallback.scale.y);
    result.scale.z =
        SanitizeFloat(result.scale.z, 0.01f, kMaxScale, fallback.scale.z);
    return result;
}

bool MatrixChanged(const DirectX::XMFLOAT4X4 &a,
                   const DirectX::XMFLOAT4X4 &b) {
    const float *lhs = &a.m[0][0];
    const float *rhs = &b.m[0][0];
    for (int i = 0; i < 16; ++i) {
        if (std::abs(lhs[i] - rhs[i]) > 0.0001f) {
            return true;
        }
    }
    return false;
}

bool CaptureSceneState(IEditableScene &scene, std::string &state) {
    std::string message;
    return scene.CaptureSceneState(&state, &message);
}

void PushSceneCommand(EditorContext &context, IEditableScene &scene,
                      const char *name, const std::string &beforeState) {
    if (!context.commands || beforeState.empty()) {
        return;
    }
    std::string afterState;
    if (!CaptureSceneState(scene, afterState) || afterState == beforeState) {
        return;
    }
    context.commands->PushExecuted(std::make_unique<SceneStateCommand>(
        scene, name, beforeState, afterState));
}

EditableRay BuildViewportRay(const EditorContext &context) {
    EditableRay ray{};
    if (!context.camera || context.viewportImageSize.x <= 0.0f ||
        context.viewportImageSize.y <= 0.0f) {
        return ray;
    }

    const DirectX::XMVECTOR nearPoint = DirectX::XMVector3Unproject(
        DirectX::XMVectorSet(context.viewportMousePosition.x,
                             context.viewportMousePosition.y, 0.0f, 1.0f),
        0.0f, 0.0f, context.viewportImageSize.x,
        context.viewportImageSize.y, 0.0f, 1.0f, context.camera->GetProj(),
        context.camera->GetView(), DirectX::XMMatrixIdentity());
    const DirectX::XMVECTOR farPoint = DirectX::XMVector3Unproject(
        DirectX::XMVectorSet(context.viewportMousePosition.x,
                             context.viewportMousePosition.y, 1.0f, 1.0f),
        0.0f, 0.0f, context.viewportImageSize.x,
        context.viewportImageSize.y, 0.0f, 1.0f, context.camera->GetProj(),
        context.camera->GetView(), DirectX::XMMatrixIdentity());
    const DirectX::XMVECTOR direction =
        DirectX::XMVector3Normalize(
            DirectX::XMVectorSubtract(farPoint, nearPoint));

    ray.origin = context.camera->GetPosition();
    DirectX::XMStoreFloat3(&ray.direction, direction);
    return ray;
}

void LogSelection(EditorContext &context, uint64_t selectedId) {
    if (!context.console || !context.scene) {
        return;
    }
    if (selectedId == 0) {
        context.console->AddLog("Selection cleared");
        return;
    }

    const int selectedIndex = context.scene->GetSelectedEditableObjectIndex();
    const IEditableObject *object =
        selectedIndex >= 0
            ? context.scene->GetEditableObject(
                  static_cast<size_t>(selectedIndex))
            : nullptr;
    if (!object) {
        context.console->AddLog("Selected object");
        return;
    }

    const EditableObjectDesc desc = object->GetEditorDesc();
    context.console->AddLog("Selected: " + desc.name);
}

void HandleViewportPicking(EditorContext &context) {
    if (!context.viewportClicked || context.gameplayMode || !context.scene ||
        !context.camera || context.viewportGizmoUsing || ImGuizmo::IsUsing() ||
        ImGuizmo::IsOver()) {
        return;
    }

    const uint64_t previousId = context.scene->GetSelectedObjectId();
    const uint64_t pickedId =
        context.scene->PickEditableObject(BuildViewportRay(context));
    context.scene->SetSelectedObjectById(pickedId);
    if (pickedId != previousId) {
        LogSelection(context, pickedId);
    }
}

} // namespace

void ViewportPanel::Draw(EditorContext &context) {
    if (gizmoOperation_ == 0) {
        gizmoOperation_ = ImGuizmo::TRANSLATE;
        gizmoMode_ = ImGuizmo::WORLD;
    }

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

    DrawGizmo(context);
    HandleViewportPicking(context);

    ImGui::End();
}

void ViewportPanel::DrawGizmo(EditorContext &context) {
    context.viewportGizmoUsing = false;

    const bool viewportActive = context.viewportHovered || context.viewportFocused;
    const bool canUseGizmo = viewportActive && !context.gameplayMode &&
                             context.scene && context.camera &&
                             context.viewportImageSize.x > 0.0f &&
                             context.viewportImageSize.y > 0.0f;
    if (!canUseGizmo) {
        return;
    }

    ImGuiIO &io = ImGui::GetIO();
    if (!io.WantTextInput) {
        if (ImGui::IsKeyPressed(ImGuiKey_W, false)) {
            gizmoOperation_ = ImGuizmo::TRANSLATE;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
            gizmoOperation_ = ImGuizmo::ROTATE;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
            gizmoOperation_ = ImGuizmo::SCALE;
        }
    }

    IEditableScene *scene = context.scene;
    if (scene->GetSelectedObjectId() == 0) {
        return;
    }

    const int selectedIndex = scene->GetSelectedEditableObjectIndex();
    IEditableObject *object = selectedIndex >= 0
                                  ? scene->GetEditableObject(
                                        static_cast<size_t>(selectedIndex))
                                  : nullptr;
    if (!object) {
        return;
    }

    const EditableObjectDesc desc = object->GetEditorDesc();
    if (!desc.editable || desc.locked || !scene->CanEditObjects()) {
        return;
    }

    EditableTransform transform = object->GetEditorTransform();
    DirectX::XMFLOAT4X4 matrix = TransformToMatrix(transform);
    const DirectX::XMFLOAT4X4 beforeMatrix = matrix;

    DirectX::XMFLOAT4X4 view{};
    DirectX::XMFLOAT4X4 projection{};
    DirectX::XMStoreFloat4x4(&view, context.camera->GetView());
    DirectX::XMStoreFloat4x4(&projection, context.camera->GetProj());

    ImGuizmo::BeginFrame();
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetRect(context.viewportImagePosition.x,
                      context.viewportImagePosition.y,
                      context.viewportImageSize.x, context.viewportImageSize.y);

    const bool manipulated = ImGuizmo::Manipulate(
        &view.m[0][0], &projection.m[0][0],
        static_cast<ImGuizmo::OPERATION>(gizmoOperation_),
        static_cast<ImGuizmo::MODE>(gizmoMode_), &matrix.m[0][0]);

    context.viewportGizmoUsing = ImGuizmo::IsUsing();
    if (context.viewportGizmoUsing && !gizmoEditActive_) {
        gizmoEditActive_ = CaptureSceneState(*scene, gizmoBeforeState_);
    }
    if (gizmoEditActive_ && !context.viewportGizmoUsing) {
        PushSceneCommand(context, *scene, "Transform Object",
                         gizmoBeforeState_);
        gizmoEditActive_ = false;
        gizmoBeforeState_.clear();
    }
    if (!manipulated || !MatrixChanged(beforeMatrix, matrix)) {
        return;
    }

    object->SetEditorTransform(MatrixToTransform(matrix, transform));
    scene->OnEditableObjectChanged(static_cast<size_t>(selectedIndex));
}
