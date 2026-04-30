#include "DebugUIRegistry.h"
#include "EngineRuntime.h"
#include <algorithm>
#include <utility>

#ifdef _DEBUG
#include "imgui.h"
#endif // _DEBUG

DebugUIRegistry &DebugUIRegistry::GetInstance() {
    static DebugUIRegistry instance;
    return instance;
}

void DebugUIRegistry::Clear() {
    items_.clear();
    groups_.clear();
}

void DebugUIRegistry::RegisterBool(const std::string &group,
                                   const std::string &label, bool *value) {
#ifdef _DEBUG
    AddItem({group, label, DebugUIValueType::Bool, value});
#endif // _DEBUG
}

void DebugUIRegistry::RegisterInt(const std::string &group,
                                  const std::string &label, int *value,
                                  int minValue, int maxValue) {
#ifdef _DEBUG
    DebugUIItem item{group, label, DebugUIValueType::Int, value};
    item.minInt = minValue;
    item.maxInt = maxValue;
    AddItem(item);
#endif // _DEBUG
}

void DebugUIRegistry::RegisterFloat(const std::string &group,
                                    const std::string &label, float *value,
                                    float minValue, float maxValue) {
#ifdef _DEBUG
    DebugUIItem item{group, label, DebugUIValueType::Float, value};
    item.minFloat = minValue;
    item.maxFloat = maxValue;
    AddItem(item);
#endif // _DEBUG
}

void DebugUIRegistry::RegisterFloat2(const std::string &group,
                                     const std::string &label, float *value,
                                     float minValue, float maxValue) {
#ifdef _DEBUG
    DebugUIItem item{group, label, DebugUIValueType::Float2, value};
    item.minFloat = minValue;
    item.maxFloat = maxValue;
    AddItem(item);
#endif // _DEBUG
}

void DebugUIRegistry::RegisterFloat3(const std::string &group,
                                     const std::string &label, float *value,
                                     float minValue, float maxValue) {
#ifdef _DEBUG
    DebugUIItem item{group, label, DebugUIValueType::Float3, value};
    item.minFloat = minValue;
    item.maxFloat = maxValue;
    AddItem(item);
#endif // _DEBUG
}

void DebugUIRegistry::RegisterFloat4(const std::string &group,
                                     const std::string &label, float *value,
                                     float minValue, float maxValue) {
#ifdef _DEBUG
    DebugUIItem item{group, label, DebugUIValueType::Float4, value};
    item.minFloat = minValue;
    item.maxFloat = maxValue;
    AddItem(item);
#endif // _DEBUG
}

void DebugUIRegistry::RegisterColor3(const std::string &group,
                                     const std::string &label, float *value) {
#ifdef _DEBUG
    AddItem({group, label, DebugUIValueType::Color3, value});
#endif // _DEBUG
}

void DebugUIRegistry::RegisterColor4(const std::string &group,
                                     const std::string &label, float *value) {
#ifdef _DEBUG
    AddItem({group, label, DebugUIValueType::Color4, value});
#endif // _DEBUG
}

void DebugUIRegistry::RegisterCombo(const std::string &group,
                                    const std::string &label, int *value,
                                    const std::vector<std::string> &labels) {
#ifdef _DEBUG
    DebugUIItem item{group, label, DebugUIValueType::Combo, value};
    item.comboLabels = labels;
    AddItem(item);
#endif // _DEBUG
}

void DebugUIRegistry::Draw() {
#ifdef _DEBUG
    const EngineRuntime &runtime = EngineRuntime::GetInstance();
    if (!runtime.IsTuningMode() || !runtime.Settings().showDebugUI) {
        return;
    }

    if (items_.empty()) {
        return;
    }

    if (ImGui::Begin("Debug UI")) {
        for (const std::string &group : groups_) {
            if (!ImGui::CollapsingHeader(group.c_str(),
                                         ImGuiTreeNodeFlags_DefaultOpen)) {
                continue;
            }

            for (const DebugUIItem &item : items_) {
                if (item.group != group || !item.value) {
                    continue;
                }

                switch (item.type) {
                case DebugUIValueType::Bool:
                    ImGui::Checkbox(item.label.c_str(),
                                    static_cast<bool *>(item.value));
                    break;
                case DebugUIValueType::Int:
                    ImGui::SliderInt(item.label.c_str(),
                                     static_cast<int *>(item.value),
                                     item.minInt, item.maxInt);
                    break;
                case DebugUIValueType::Float:
                    ImGui::SliderFloat(item.label.c_str(),
                                       static_cast<float *>(item.value),
                                       item.minFloat, item.maxFloat);
                    break;
                case DebugUIValueType::Float2:
                    ImGui::DragFloat2(item.label.c_str(),
                                      static_cast<float *>(item.value), 0.01f,
                                      item.minFloat, item.maxFloat);
                    break;
                case DebugUIValueType::Float3:
                    ImGui::DragFloat3(item.label.c_str(),
                                      static_cast<float *>(item.value), 0.01f,
                                      item.minFloat, item.maxFloat);
                    break;
                case DebugUIValueType::Float4:
                    ImGui::DragFloat4(item.label.c_str(),
                                      static_cast<float *>(item.value), 0.01f,
                                      item.minFloat, item.maxFloat);
                    break;
                case DebugUIValueType::Color3:
                    ImGui::ColorEdit3(item.label.c_str(),
                                      static_cast<float *>(item.value));
                    break;
                case DebugUIValueType::Color4:
                    ImGui::ColorEdit4(item.label.c_str(),
                                      static_cast<float *>(item.value));
                    break;
                case DebugUIValueType::Combo: {
                    std::vector<const char *> labels;
                    labels.reserve(item.comboLabels.size());
                    for (const std::string &comboLabel : item.comboLabels) {
                        labels.push_back(comboLabel.c_str());
                    }
                    if (!labels.empty()) {
                        ImGui::Combo(item.label.c_str(),
                                     static_cast<int *>(item.value),
                                     labels.data(),
                                     static_cast<int>(labels.size()));
                    }
                    break;
                }
                }
            }
        }
    }
    ImGui::End();
#endif // _DEBUG
}

void DebugUIRegistry::AddItem(DebugUIItem item) {
    const bool hasGroup =
        std::find(groups_.begin(), groups_.end(), item.group) != groups_.end();
    if (!hasGroup) {
        groups_.push_back(item.group);
    }
    items_.push_back(std::move(item));
}
