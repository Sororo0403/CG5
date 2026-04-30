#include "panels/InspectorPanel.h"
#include "EditorContext.h"
#include "IEditableScene.h"
#include "imgui.h"
#include <cmath>
#include <cstdio>

namespace {

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

    const int selectedIndex = scene->GetSelectedEditableObjectIndex();
    IEditableObject *object =
        scene->GetEditableObject(static_cast<size_t>(selectedIndex));
    if (!object) {
        ImGui::Text("No object selected");
        ImGui::End();
        return;
    }

    const EditableObjectDesc desc = object->GetEditorDesc();
    char nameBuffer[128]{};
    std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", desc.name.c_str());
    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
        object->SetEditorName(nameBuffer);
    }

    ImGui::Text("Type: %s", desc.type.c_str());
    ImGui::Separator();

    EditableTransform transform = object->GetEditorTransform();
    bool changed = false;
    changed |= ImGui::DragFloat3("Position", &transform.position.x, 0.05f);
    changed |= ImGui::DragFloat4("Rotation", &transform.rotation.x, 0.005f);
    changed |= ImGui::DragFloat3("Scale", &transform.scale.x, 0.02f);
    if (changed) {
        NormalizeQuaternion(transform.rotation);
        object->SetEditorTransform(transform);
        scene->OnEditableObjectChanged(static_cast<size_t>(selectedIndex));
    }

    ImGui::Separator();
    if (!desc.collider.empty()) {
        ImGui::Text("Collider: %s", desc.collider.c_str());
    } else {
        ImGui::Text("Collider: none");
    }

    ImGui::End();
}
