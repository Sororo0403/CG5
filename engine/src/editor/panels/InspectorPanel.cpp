#include "panels/InspectorPanel.h"
#include "EditorContext.h"
#include "IEditableScene.h"
#include <DirectXMath.h>
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

constexpr float kRadToDeg = 57.29577951308232f;
constexpr float kDegToRad = 0.017453292519943295f;
constexpr float kMaxPosition = 10000.0f;
constexpr float kMaxScale = 10000.0f;

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
    EditableTransform transform = object->GetEditorTransform();
    DirectX::XMFLOAT3 eulerDegrees =
        QuaternionToEulerDegrees(transform.rotation);
    if (context.readOnly) {
        ImGui::TextDisabled("Read Only - Gameplay Mode");
        ImGui::Text("Name: %s", desc.name.c_str());
        ImGui::Text("Type: %s", desc.type.c_str());
        ImGui::Text("ID: %llu", static_cast<unsigned long long>(desc.id));
        ImGui::Separator();
        ImGui::Text("Position: %.3f, %.3f, %.3f", transform.position.x,
                    transform.position.y, transform.position.z);
        ImGui::Text("Rotation: %.2f, %.2f, %.2f deg", eulerDegrees.x,
                    eulerDegrees.y, eulerDegrees.z);
        ImGui::Text("Scale: %.3f, %.3f, %.3f", transform.scale.x,
                    transform.scale.y, transform.scale.z);
    } else {
        char nameBuffer[128]{};
        std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", desc.name.c_str());
        if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
            object->SetEditorName(nameBuffer);
            scene->OnEditableObjectChanged(static_cast<size_t>(selectedIndex));
        }

        ImGui::Text("Type: %s", desc.type.c_str());
        ImGui::Text("ID: %llu", static_cast<unsigned long long>(desc.id));
        ImGui::Separator();

        bool changed = false;
        changed |= ImGui::DragFloat3("Position", &transform.position.x, 0.05f);
        if (ImGui::DragFloat3("Rotation", &eulerDegrees.x, 0.25f)) {
            transform.rotation = EulerDegreesToQuaternion(eulerDegrees);
            changed = true;
        }
        changed |= ImGui::DragFloat3("Scale", &transform.scale.x, 0.02f);
        if (changed) {
            SanitizeTransform(transform);
            object->SetEditorTransform(transform);
            scene->OnEditableObjectChanged(static_cast<size_t>(selectedIndex));
        }
    }

    ImGui::Separator();
    if (!desc.warning.empty()) {
        ImGui::TextWrapped("Warning: %s", desc.warning.c_str());
    }
    if (!desc.collider.empty()) {
        ImGui::Text("Collider: %s", desc.collider.c_str());
    } else {
        ImGui::Text("Collider: none");
    }

    ImGui::End();
}
