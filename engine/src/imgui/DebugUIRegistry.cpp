#include "DebugUIRegistry.h"
#include "EngineRuntime.h"
#include "Transform.h"
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
    RegisterSliderInt(group, label, value, minValue, maxValue);
}

void DebugUIRegistry::RegisterSliderInt(const std::string &group,
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
    RegisterSliderFloat(group, label, value, minValue, maxValue);
}

void DebugUIRegistry::RegisterSliderFloat(const std::string &group,
                                          const std::string &label,
                                          float *value, float minValue,
                                          float maxValue) {
#ifdef _DEBUG
    DebugUIItem item{group, label, DebugUIValueType::Float, value};
    item.minFloat = minValue;
    item.maxFloat = maxValue;
    AddItem(item);
#endif // _DEBUG
}

void DebugUIRegistry::RegisterDragFloat(const std::string &group,
                                        const std::string &label, float *value,
                                        float speed, float minValue,
                                        float maxValue) {
#ifdef _DEBUG
    DebugUIItem item{group, label, DebugUIValueType::Float, value};
    item.minFloat = minValue;
    item.maxFloat = maxValue;
    item.speed = speed;
    item.control = DebugUIControlType::Drag;
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

void DebugUIRegistry::RegisterDragFloat3(const std::string &group,
                                         const std::string &label,
                                         float *value, float speed) {
#ifdef _DEBUG
    DebugUIItem item{group, label, DebugUIValueType::Float3, value};
    item.speed = speed;
    item.control = DebugUIControlType::Drag;
    AddItem(item);
#endif // _DEBUG
}

void DebugUIRegistry::RegisterTransform(const std::string &group,
                                        const std::string &label,
                                        Transform *transform) {
#ifdef _DEBUG
    if (!transform) {
        return;
    }

    RegisterDragFloat3(group, label + " Position", &transform->position.x,
                       0.05f);
    RegisterDragFloat3(group, label + " Scale", &transform->scale.x, 0.01f);
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

void DebugUIRegistry::RegisterCombo(
    const std::string &group, const std::string &label, int *value,
    std::initializer_list<const char *> labels) {
#ifdef _DEBUG
    std::vector<std::string> stringLabels;
    stringLabels.reserve(labels.size());
    for (const char *labelText : labels) {
        stringLabels.emplace_back(labelText ? labelText : "");
    }
    RegisterCombo(group, label, value, stringLabels);
#endif // _DEBUG
}

void DebugUIRegistry::RegisterText(const std::string &group,
                                   const std::string &label,
                                   std::function<std::string()> getter) {
#ifdef _DEBUG
    DebugUIItem item{group, label, DebugUIValueType::Text};
    item.textGetter = std::move(getter);
    AddItem(item);
#endif // _DEBUG
}

void DebugUIRegistry::RegisterReadonlyFloat(const std::string &group,
                                            const std::string &label,
                                            std::function<float()> getter) {
#ifdef _DEBUG
    DebugUIItem item{group, label, DebugUIValueType::ReadonlyFloat};
    item.floatGetter = std::move(getter);
    AddItem(item);
#endif // _DEBUG
}

void DebugUIRegistry::RegisterReadonlyInt(const std::string &group,
                                          const std::string &label,
                                          std::function<int()> getter) {
#ifdef _DEBUG
    DebugUIItem item{group, label, DebugUIValueType::ReadonlyInt};
    item.intGetter = std::move(getter);
    AddItem(item);
#endif // _DEBUG
}

void DebugUIRegistry::SetVisible(bool visible) { visible_ = visible; }

bool DebugUIRegistry::IsVisible() const { return visible_; }

void DebugUIRegistry::Draw() {
#ifdef _DEBUG
    const EngineRuntime &runtime = EngineRuntime::GetInstance();
    if (!runtime.IsTuningMode() || !runtime.Settings().showDebugUI ||
        !visible_) {
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
                if (item.group != group) {
                    continue;
                }

                switch (item.type) {
                case DebugUIValueType::Bool:
                    if (item.value) {
                        ImGui::Checkbox(item.label.c_str(),
                                        static_cast<bool *>(item.value));
                    }
                    break;
                case DebugUIValueType::Int:
                    if (item.value) {
                        ImGui::SliderInt(item.label.c_str(),
                                         static_cast<int *>(item.value),
                                         item.minInt, item.maxInt);
                    }
                    break;
                case DebugUIValueType::Float:
                    if (item.value) {
                        if (item.control == DebugUIControlType::Drag) {
                            ImGui::DragFloat(item.label.c_str(),
                                             static_cast<float *>(item.value),
                                             item.speed, item.minFloat,
                                             item.maxFloat);
                        } else {
                            ImGui::SliderFloat(
                                item.label.c_str(),
                                static_cast<float *>(item.value),
                                item.minFloat, item.maxFloat);
                        }
                    }
                    break;
                case DebugUIValueType::Float2:
                    if (item.value) {
                        ImGui::DragFloat2(item.label.c_str(),
                                          static_cast<float *>(item.value),
                                          item.speed, item.minFloat,
                                          item.maxFloat);
                    }
                    break;
                case DebugUIValueType::Float3:
                    if (item.value) {
                        ImGui::DragFloat3(item.label.c_str(),
                                          static_cast<float *>(item.value),
                                          item.speed, item.minFloat,
                                          item.maxFloat);
                    }
                    break;
                case DebugUIValueType::Float4:
                    if (item.value) {
                        ImGui::DragFloat4(item.label.c_str(),
                                          static_cast<float *>(item.value),
                                          item.speed, item.minFloat,
                                          item.maxFloat);
                    }
                    break;
                case DebugUIValueType::Color3:
                    if (item.value) {
                        ImGui::ColorEdit3(item.label.c_str(),
                                          static_cast<float *>(item.value));
                    }
                    break;
                case DebugUIValueType::Color4:
                    if (item.value) {
                        ImGui::ColorEdit4(item.label.c_str(),
                                          static_cast<float *>(item.value));
                    }
                    break;
                case DebugUIValueType::Combo: {
                    if (item.value) {
                        std::vector<const char *> labels;
                        labels.reserve(item.comboLabels.size());
                        for (const std::string &comboLabel :
                             item.comboLabels) {
                            labels.push_back(comboLabel.c_str());
                        }
                        if (!labels.empty()) {
                            ImGui::Combo(item.label.c_str(),
                                         static_cast<int *>(item.value),
                                         labels.data(),
                                         static_cast<int>(labels.size()));
                        }
                    }
                    break;
                }
                case DebugUIValueType::Text:
                    if (item.textGetter) {
                        ImGui::Text("%s: %s", item.label.c_str(),
                                    item.textGetter().c_str());
                    }
                    break;
                case DebugUIValueType::ReadonlyFloat:
                    if (item.floatGetter) {
                        ImGui::Text("%s: %.3f", item.label.c_str(),
                                    item.floatGetter());
                    }
                    break;
                case DebugUIValueType::ReadonlyInt:
                    if (item.intGetter) {
                        ImGui::Text("%s: %d", item.label.c_str(),
                                    item.intGetter());
                    }
                    break;
                }
            }
        }
    }
    ImGui::End();
#endif // _DEBUG
}

void DebugUIRegistry::AddItem(DebugUIItem item) {
    // 同じgroup+labelは置き換える。Scene初期化の再実行で同じUIが増えるのを防ぐ。
    auto existing = std::find_if(
        items_.begin(), items_.end(), [&item](const DebugUIItem &registered) {
            return registered.group == item.group &&
                   registered.label == item.label;
        });
    if (existing != items_.end()) {
        *existing = std::move(item);
        return;
    }

    const bool hasGroup =
        std::find(groups_.begin(), groups_.end(), item.group) != groups_.end();
    if (!hasGroup) {
        groups_.push_back(item.group);
    }
    items_.push_back(std::move(item));
}
