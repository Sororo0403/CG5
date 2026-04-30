#include "EditorCommand.h"
#include "panels/InspectorPanel.h"
#include "EditorContext.h"
#include "IEditableScene.h"
#include "imgui.h"
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <memory>

namespace {

constexpr float kRadToDeg = 57.29577951308232f;
constexpr float kDegToRad = 0.017453292519943295f;
constexpr float kMaxPosition = 10000.0f;
constexpr float kMaxScale = 10000.0f;

constexpr const char *kColliderOptions[] = {"None", "Walkable", "Blocked"};

void NormalizeQuaternion(DirectX::XMFLOAT4 &rotation) {
    const float lengthSq = rotation.x * rotation.x + rotation.y * rotation.y +
                           rotation.z * rotation.z + rotation.w * rotation.w;
    if (lengthSq <= 0.000001f) {
        rotation = {0.0f, 0.0f, 0.0f, 1.0f};
        return;
    }

    const float invLength = 1.0f / std::sqrt(lengthSq);
    rotation.x *= invLength;
    rotation.y *= invLength;
    rotation.z *= invLength;
    rotation.w *= invLength;
}

DirectX::XMFLOAT3 QuaternionToEulerDegrees(DirectX::XMFLOAT4 rotation) {
    NormalizeQuaternion(rotation);

    const float sinPitchCos = 2.0f * (rotation.w * rotation.x +
                                      rotation.y * rotation.z);
    const float cosPitchCos = 1.0f - 2.0f * (rotation.x * rotation.x +
                                             rotation.y * rotation.y);
    const float pitch = std::atan2(sinPitchCos, cosPitchCos);

    const float sinYaw =
        2.0f * (rotation.w * rotation.y - rotation.z * rotation.x);
    const float yaw =
        std::abs(sinYaw) >= 1.0f
            ? std::copysign(DirectX::XM_PIDIV2, sinYaw)
            : std::asin(sinYaw);

    const float sinRollCos = 2.0f * (rotation.w * rotation.z +
                                     rotation.x * rotation.y);
    const float cosRollCos = 1.0f - 2.0f * (rotation.y * rotation.y +
                                            rotation.z * rotation.z);
    const float roll = std::atan2(sinRollCos, cosRollCos);

    return {pitch * kRadToDeg, yaw * kRadToDeg, roll * kRadToDeg};
}

DirectX::XMFLOAT4 EulerDegreesToQuaternion(
    const DirectX::XMFLOAT3 &eulerDegrees) {
    DirectX::XMFLOAT4 result{};
    DirectX::XMStoreFloat4(
        &result, DirectX::XMQuaternionRotationRollPitchYaw(
                     eulerDegrees.x * kDegToRad, eulerDegrees.y * kDegToRad,
                     eulerDegrees.z * kDegToRad));
    NormalizeQuaternion(result);
    return result;
}

float SanitizeFloat(float value, float minValue, float maxValue,
                    float fallback) {
    if (!std::isfinite(value)) {
        return fallback;
    }
    return (std::clamp)(value, minValue, maxValue);
}

void SanitizeTransform(EditableTransform &transform) {
    transform.position.x =
        SanitizeFloat(transform.position.x, -kMaxPosition, kMaxPosition, 0.0f);
    transform.position.y =
        SanitizeFloat(transform.position.y, -kMaxPosition, kMaxPosition, 0.0f);
    transform.position.z =
        SanitizeFloat(transform.position.z, -kMaxPosition, kMaxPosition, 0.0f);

    NormalizeQuaternion(transform.rotation);

    transform.scale.x =
        SanitizeFloat(transform.scale.x, 0.01f, kMaxScale, 1.0f);
    transform.scale.y =
        SanitizeFloat(transform.scale.y, 0.01f, kMaxScale, 1.0f);
    transform.scale.z =
        SanitizeFloat(transform.scale.z, 0.01f, kMaxScale, 1.0f);
}

int ColliderIndex(const std::string &collider) {
    for (int i = 0; i < static_cast<int>(std::size(kColliderOptions)); ++i) {
        if (collider == kColliderOptions[i]) {
            return i;
        }
    }
    return 0;
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

} // namespace

void InspectorPanel::Draw(EditorContext &context) {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoCollapse;
    if (!ImGui::Begin("Inspector", nullptr, flags)) {
        ImGui::End();
        return;
    }

    IEditableScene *scene = context.scene;
    if (!scene) {
        ImGui::Text("No editable scene");
        ImGui::End();
        return;
    }

    const uint64_t selectedObjectId = scene->GetSelectedObjectId();
    if (selectedObjectId == 0) {
        ImGui::Text("No object selected");
        ImGui::End();
        return;
    }

    const int selectedIndex = scene->GetSelectedEditableObjectIndex();
    IEditableObject *object = selectedIndex >= 0
                                  ? scene->GetEditableObject(
                                        static_cast<size_t>(selectedIndex))
                                  : nullptr;
    if (!object) {
        ImGui::Text("No object selected");
        ImGui::End();
        return;
    }

    const EditableObjectDesc desc = object->GetEditorDesc();
    SyncNameBuffer(desc);

    const bool readOnly = context.readOnly || !desc.editable;
    if (context.readOnly) {
        ImGui::TextDisabled("Read Only - Gameplay Mode");
    } else if (!desc.editable) {
        ImGui::TextDisabled("Read Only - Object is not editable");
    }

    if (readOnly) {
        ImGui::BeginDisabled();
    }

    ImGui::SeparatorText("Basic");
    ImGui::SetNextItemWidth(-1.0f);
    const bool nameEnter = ImGui::InputText(
        "Name", nameBuffer_, static_cast<size_t>(std::size(nameBuffer_)),
        ImGuiInputTextFlags_EnterReturnsTrue);
    if (nameEnter || ImGui::IsItemDeactivatedAfterEdit()) {
        CommitName(*scene, *object, static_cast<size_t>(selectedIndex));
    }
    ImGui::Text("Type: %s", desc.type.c_str());
    ImGui::Text("ID: %llu", static_cast<unsigned long long>(desc.id));

    ImGui::SeparatorText("Properties");
    bool locked = desc.locked;
    if (ImGui::Checkbox("Locked", &locked)) {
        std::string beforeState;
        CaptureSceneState(*scene, beforeState);
        if (object->SetEditorLocked(locked)) {
            scene->OnEditableObjectChanged(static_cast<size_t>(selectedIndex));
            PushSceneCommand(context, *scene, "Edit Object", beforeState);
        }
    }
    bool visible = desc.visible;
    if (ImGui::Checkbox("Visible", &visible)) {
        std::string beforeState;
        CaptureSceneState(*scene, beforeState);
        if (object->SetEditorVisible(visible)) {
            scene->OnEditableObjectChanged(static_cast<size_t>(selectedIndex));
            PushSceneCommand(context, *scene, "Edit Object", beforeState);
        }
    }

    int colliderIndex = ColliderIndex(desc.collider);
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::Combo("Collider", &colliderIndex, kColliderOptions,
                     static_cast<int>(std::size(kColliderOptions)))) {
        std::string beforeState;
        CaptureSceneState(*scene, beforeState);
        if (object->SetEditorCollider(kColliderOptions[colliderIndex])) {
            scene->OnEditableObjectChanged(static_cast<size_t>(selectedIndex));
            PushSceneCommand(context, *scene, "Edit Object", beforeState);
        }
    }

    ImGui::SeparatorText("Transform");
    if (desc.locked) {
        ImGui::BeginDisabled();
    }
    EditableTransform transform = object->GetEditorTransform();
    DirectX::XMFLOAT3 eulerDegrees =
        QuaternionToEulerDegrees(transform.rotation);
    bool transformChanged = false;
    bool transformControlActive = false;
    transformChanged |= ImGui::DragFloat3("Position", &transform.position.x, 0.05f);
    transformControlActive |= ImGui::IsItemActive();
    if (ImGui::DragFloat3("Rotation", &eulerDegrees.x, 0.25f)) {
        transform.rotation = EulerDegreesToQuaternion(eulerDegrees);
        transformChanged = true;
    }
    transformControlActive |= ImGui::IsItemActive();
    transformChanged |= ImGui::DragFloat3("Scale", &transform.scale.x, 0.02f);
    transformControlActive |= ImGui::IsItemActive();
    if (desc.locked) {
        ImGui::EndDisabled();
    }
    if (transformChanged && !desc.locked) {
        if (!transformEditActive_) {
            transformEditActive_ =
                CaptureSceneState(*scene, transformBeforeState_);
        }
        SanitizeTransform(transform);
        object->SetEditorTransform(transform);
        scene->OnEditableObjectChanged(static_cast<size_t>(selectedIndex));
    }
    if (transformEditActive_ && !transformControlActive) {
        PushSceneCommand(context, *scene, "Transform Object",
                         transformBeforeState_);
        transformEditActive_ = false;
        transformBeforeState_.clear();
    }

    if (readOnly) {
        ImGui::EndDisabled();
    }

    ImGui::SeparatorText("Debug");
    if (!desc.warning.empty()) {
        const ImVec4 warningColor{1.0f, 0.24f, 0.18f, 1.0f};
        ImGui::PushStyleColor(ImGuiCol_Text, warningColor);
        ImGui::TextWrapped("Warning: %s", desc.warning.c_str());
        ImGui::PopStyleColor();
    } else {
        ImGui::TextDisabled("Warning: none");
    }
    ImGui::Text("Editable: %s", desc.editable ? "true" : "false");
    ImGui::Text("Selected index: %d", selectedIndex);
    ImGui::Text("Collider: %s", desc.collider.empty() ? "None"
                                                       : desc.collider.c_str());

    ImGui::End();
}

void InspectorPanel::SyncNameBuffer(const EditableObjectDesc &desc) {
    if (editingObjectId_ == desc.id) {
        return;
    }
    transformEditActive_ = false;
    transformBeforeState_.clear();
    editingObjectId_ = desc.id;
    std::snprintf(nameBuffer_, sizeof(nameBuffer_), "%s", desc.name.c_str());
}

void InspectorPanel::CommitName(IEditableScene &scene, IEditableObject &object,
                                size_t selectedIndex) {
    const EditableObjectDesc desc = object.GetEditorDesc();
    if (std::strcmp(nameBuffer_, desc.name.c_str()) == 0) {
        return;
    }
    object.SetEditorName(nameBuffer_);
    scene.OnEditableObjectChanged(selectedIndex);
}
