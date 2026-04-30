#include "panels/HierarchyPanel.h"
#include "EditorContext.h"
#include "EditorConsole.h"
#include "IEditableScene.h"
#include "imgui.h"
#include <cstdio>
#include <iterator>
#include <string>

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

    static int addTypeIndex = 0;
    constexpr const char *kAddTypes[] = {"Floor", "Wall", "PlayerStart",
                                         "GoalMarker"};
    const bool canEdit = !context.readOnly && scene->CanEditObjects();
    if (!canEdit) {
        ImGui::BeginDisabled();
    }

    ImGui::SetNextItemWidth(130.0f);
    ImGui::Combo("##AddObjectType", &addTypeIndex, kAddTypes,
                 static_cast<int>(std::size(kAddTypes)));
    ImGui::SameLine();
    if (ImGui::Button("Add")) {
        std::string message;
        const bool added =
            scene->AddEditableObject(kAddTypes[addTypeIndex], &message);
        if (context.console) {
            context.console->AddLog(added ? message
                                          : "Failed to add object: " + message);
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Duplicate")) {
        std::string message;
        const bool duplicated =
            scene->DuplicateSelectedEditableObject(&message);
        if (context.console) {
            context.console->AddLog(duplicated
                                        ? message
                                        : "Failed to duplicate object: " +
                                              message);
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Delete")) {
        std::string message;
        const bool deleted = scene->DeleteSelectedEditableObject(&message);
        if (context.console) {
            context.console->AddLog(deleted ? message
                                            : "Failed to delete object: " +
                                                  message);
        }
    }

    if (!canEdit) {
        ImGui::EndDisabled();
    }
    ImGui::Separator();

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
