#include "panels/HierarchyPanel.h"
#include "EditorConsole.h"
#include "EditorContext.h"
#include "IEditableScene.h"
#include "imgui.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <iterator>
#include <string>

namespace {

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::tolower(c));
                   });
    return value;
}

bool NameMatchesFilter(const std::string &name, const char *filter) {
    if (!filter || filter[0] == '\0') {
        return true;
    }
    return ToLower(name).find(ToLower(filter)) != std::string::npos;
}

void LogResult(EditorContext &context, bool ok, const std::string &message,
               const char *failedPrefix) {
    if (!context.console) {
        return;
    }
    context.console->AddLog(ok ? message : failedPrefix + message);
}

} // namespace

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
        LogResult(context, added, message, "Failed to add object: ");
    }

    if (!canEdit) {
        ImGui::EndDisabled();
    }

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##HierarchySearch", searchBuffer_,
                     static_cast<size_t>(std::size(searchBuffer_)));
    ImGui::Separator();

    bool clickedOnItem = false;
    if (ImGui::BeginChild("HierarchyList", {0.0f, 0.0f}, false)) {
        for (size_t index = 0; index < scene->GetEditableObjectCount();
             ++index) {
            IEditableObject *object = scene->GetEditableObject(index);
            if (!object) {
                continue;
            }

            const EditableObjectDesc desc = object->GetEditorDesc();
            if (!NameMatchesFilter(desc.name, searchBuffer_)) {
                continue;
            }

            clickedOnItem |= DrawObjectRow(context, *scene, *object, index);
        }

        if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
            !clickedOnItem) {
            scene->SetSelectedObjectById(0);
            renamingObjectId_ = 0;
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

void HierarchyPanel::BeginRename(const EditableObjectDesc &desc) {
    renamingObjectId_ = desc.id;
    renameFocusPending_ = true;
    std::snprintf(renameBuffer_, sizeof(renameBuffer_), "%s",
                  desc.name.c_str());
}

bool HierarchyPanel::DrawObjectRow(EditorContext &context,
                                   IEditableScene &scene,
                                   IEditableObject &object, size_t index) {
    const EditableObjectDesc desc = object.GetEditorDesc();
    const bool canEdit = !context.readOnly && scene.CanEditObjects();
    const bool selected = scene.GetSelectedObjectId() == desc.id;
    bool clickedOnItem = false;

    char idScope[32]{};
    std::snprintf(idScope, sizeof(idScope), "%llu",
                  static_cast<unsigned long long>(desc.id));
    ImGui::PushID(idScope);

    if (renamingObjectId_ == desc.id && canEdit) {
        ImGui::SetNextItemWidth(-1.0f);
        if (renameFocusPending_) {
            ImGui::SetKeyboardFocusHere();
            renameFocusPending_ = false;
        }
        const bool enterPressed = ImGui::InputText(
            "##Rename", renameBuffer_, static_cast<size_t>(std::size(renameBuffer_)),
            ImGuiInputTextFlags_EnterReturnsTrue |
                ImGuiInputTextFlags_AutoSelectAll);
        clickedOnItem |= ImGui::IsItemClicked() || ImGui::IsItemActive();
        if (enterPressed || ImGui::IsItemDeactivatedAfterEdit()) {
            object.SetEditorName(renameBuffer_);
            scene.SetSelectedObjectById(desc.id);
            scene.OnEditableObjectChanged(index);
            renamingObjectId_ = 0;
            renameFocusPending_ = false;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            renamingObjectId_ = 0;
            renameFocusPending_ = false;
        }
    } else {
        char label[192]{};
        std::snprintf(label, sizeof(label), "%s (ID: %llu)",
                      desc.name.c_str(),
                      static_cast<unsigned long long>(desc.id));
        if (ImGui::Selectable(label, selected,
                              ImGuiSelectableFlags_AllowDoubleClick |
                                  ImGuiSelectableFlags_SpanAllColumns)) {
            scene.SetSelectedObjectById(desc.id);
            clickedOnItem = true;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && canEdit) {
                BeginRename(desc);
            }
        }
        clickedOnItem |= ImGui::IsItemHovered();

        if (ImGui::BeginPopupContextItem("ObjectContext")) {
            scene.SetSelectedObjectById(desc.id);
            if (!canEdit) {
                ImGui::BeginDisabled();
            }
            if (ImGui::MenuItem("Rename")) {
                BeginRename(desc);
            }
            if (ImGui::MenuItem("Duplicate")) {
                std::string message;
                const bool duplicated =
                    scene.DuplicateSelectedEditableObject(&message);
                LogResult(context, duplicated, message,
                          "Failed to duplicate object: ");
            }
            if (ImGui::MenuItem("Delete")) {
                std::string message;
                const bool deleted =
                    scene.DeleteSelectedEditableObject(&message);
                LogResult(context, deleted, message,
                          "Failed to delete object: ");
            }
            if (!canEdit) {
                ImGui::EndDisabled();
            }
            ImGui::EndPopup();
            clickedOnItem = true;
        }

        ImGui::TextDisabled("  %s", desc.type.c_str());
    }

    ImGui::PopID();
    return clickedOnItem;
}
