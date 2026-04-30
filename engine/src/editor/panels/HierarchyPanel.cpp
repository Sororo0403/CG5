#include "panels/HierarchyPanel.h"
#include "EditorContext.h"
#include "IEditableScene.h"
#include "imgui.h"
#include <cstdio>

void HierarchyPanel::Draw(EditorContext &context) {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoCollapse;
    if (!ImGui::Begin("Hierarchy", nullptr, flags)) {
        ImGui::End();
        return;
    }

    IEditableScene *scene = context.scene;
    if (!scene) {
        ImGui::Text("No editable scene");
        ImGui::End();
        return;
    }

    for (int index = 0; index < static_cast<int>(scene->GetEditableObjectCount());
         ++index) {
        const IEditableObject *object =
            scene->GetEditableObject(static_cast<size_t>(index));
        if (!object) {
            continue;
        }

        const EditableObjectDesc desc = object->GetEditorDesc();
        char label[160]{};
        std::snprintf(label, sizeof(label), "%03d  %s", index,
                      desc.name.c_str());
        if (ImGui::Selectable(label,
                              scene->GetSelectedEditableObjectIndex() == index)) {
            scene->SetSelectedEditableObjectIndex(index);
        }
    }

    ImGui::End();
}
