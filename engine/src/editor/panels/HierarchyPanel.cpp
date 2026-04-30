#include "panels/HierarchyPanel.h"
#include "EditableObjectFactory.h"
#include "EditorCommand.h"
#include "EditorConsole.h"
#include "EditorContext.h"
#include "IEditableScene.h"
#include "imgui.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <memory>
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

void LogLockedObject(EditorContext &context, const EditableObjectDesc &desc) {
    if (context.console) {
        context.console->AddLog("Object is locked: " + desc.name);
    }
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

    const std::vector<std::string> &addTypes =
        EditableObjectFactory::GetInstance().GetRegisteredTypes();
    if (addTypeIndex_ >= static_cast<int>(addTypes.size())) {
        addTypeIndex_ = 0;
    }
    const bool canEdit = !context.readOnly && scene->CanEditObjects();
    if (!canEdit || addTypes.empty()) {
        ImGui::BeginDisabled();
    }

    ImGui::SetNextItemWidth(130.0f);
    const char *preview =
        addTypes.empty() ? "No types" : addTypes[static_cast<size_t>(addTypeIndex_)].c_str();
    if (ImGui::BeginCombo("##AddObjectType", preview)) {
        for (int index = 0; index < static_cast<int>(addTypes.size()); ++index) {
            const bool selected = addTypeIndex_ == index;
            if (ImGui::Selectable(addTypes[static_cast<size_t>(index)].c_str(),
                                  selected)) {
                addTypeIndex_ = index;
            }
            if (selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Add")) {
        std::string beforeState;
        CaptureSceneState(*scene, beforeState);
        std::string message;
        const std::string &type = addTypes[static_cast<size_t>(addTypeIndex_)];
        const bool added = scene->AddEditableObject(type, &message);
        if (added) {
            PushSceneCommand(context, *scene, "Add Object", beforeState);
        }
        LogResult(context, added, message, "Failed to add object: ");
    }

    if (!canEdit || addTypes.empty()) {
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
    const bool canModifyObject = canEdit && !desc.locked;
    const bool selected = scene.GetSelectedObjectId() == desc.id;
    bool clickedOnItem = false;

    char idScope[32]{};
    std::snprintf(idScope, sizeof(idScope), "%llu",
                  static_cast<unsigned long long>(desc.id));
    ImGui::PushID(idScope);

    if (renamingObjectId_ == desc.id && canModifyObject) {
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
            if (std::strcmp(renameBuffer_, desc.name.c_str()) != 0) {
                std::string beforeState;
                CaptureSceneState(scene, beforeState);
                object.SetEditorName(renameBuffer_);
                scene.OnEditableObjectChanged(index);
                PushSceneCommand(context, scene, "Rename Object",
                                 beforeState);
            }
            scene.SetSelectedObjectById(desc.id);
            renamingObjectId_ = 0;
            renameFocusPending_ = false;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            renamingObjectId_ = 0;
            renameFocusPending_ = false;
        }
    } else {
        char label[192]{};
        char status[64]{};
        if (!desc.visible) {
            std::snprintf(status + std::strlen(status),
                          sizeof(status) - std::strlen(status), " [hidden]");
        }
        if (desc.locked) {
            std::snprintf(status + std::strlen(status),
                          sizeof(status) - std::strlen(status), " [locked]");
        }
        std::snprintf(label, sizeof(label), "%s%s (ID: %llu)",
                      desc.name.c_str(), status,
                      static_cast<unsigned long long>(desc.id));
        if (!desc.visible) {
            const ImVec4 disabledColor =
                ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
            ImGui::PushStyleColor(ImGuiCol_Text, disabledColor);
        }
        if (ImGui::Selectable(label, selected,
                              ImGuiSelectableFlags_AllowDoubleClick |
                                  ImGuiSelectableFlags_SpanAllColumns)) {
            scene.SetSelectedObjectById(desc.id);
            clickedOnItem = true;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
                canEdit) {
                if (desc.locked) {
                    LogLockedObject(context, desc);
                } else {
                    BeginRename(desc);
                }
            }
        }
        if (!desc.visible) {
            ImGui::PopStyleColor();
        }
        if (renamingObjectId_ == desc.id && desc.locked) {
            renamingObjectId_ = 0;
            renameFocusPending_ = false;
        }
        clickedOnItem |= ImGui::IsItemHovered();

        if (ImGui::BeginPopupContextItem("ObjectContext")) {
            scene.SetSelectedObjectById(desc.id);
            if (!canModifyObject) {
                ImGui::BeginDisabled();
            }
            if (ImGui::MenuItem("Rename")) {
                BeginRename(desc);
            }
            if (ImGui::MenuItem("Duplicate")) {
                std::string beforeState;
                CaptureSceneState(scene, beforeState);
                std::string message;
                const bool duplicated =
                    scene.DuplicateSelectedEditableObject(&message);
                if (duplicated) {
                    PushSceneCommand(context, scene, "Duplicate Object",
                                     beforeState);
                }
                LogResult(context, duplicated, message,
                          "Failed to duplicate object: ");
            }
            if (ImGui::MenuItem("Delete")) {
                std::string beforeState;
                CaptureSceneState(scene, beforeState);
                std::string message;
                const bool deleted =
                    scene.DeleteSelectedEditableObject(&message);
                if (deleted) {
                    PushSceneCommand(context, scene, "Delete Object",
                                     beforeState);
                }
                LogResult(context, deleted, message,
                          "Failed to delete object: ");
            }
            if (!canModifyObject) {
                ImGui::EndDisabled();
            }
            ImGui::EndPopup();
            clickedOnItem = true;
        }

        if (desc.visible) {
            ImGui::TextDisabled("  %s", desc.type.c_str());
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::Text("  %s", desc.type.c_str());
            ImGui::PopStyleColor();
        }
    }

    ImGui::PopID();
    return clickedOnItem;
}
