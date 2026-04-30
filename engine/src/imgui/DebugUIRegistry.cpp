#include "DebugUIRegistry.h"
#include "EngineRuntime.h"
#include "Transform.h"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <utility>

#ifdef _DEBUG
#include "imgui.h"
#endif // _DEBUG

namespace {
constexpr const char *kDefaultTuningPath = "resources/settings/debug_tuning.json";

bool IsFloatArrayType(DebugUIValueType type) {
    return type == DebugUIValueType::Float2 ||
           type == DebugUIValueType::Float3 ||
           type == DebugUIValueType::Float4 ||
           type == DebugUIValueType::Color3 ||
           type == DebugUIValueType::Color4;
}
} // namespace

DebugUIRegistry &DebugUIRegistry::GetInstance() {
    static DebugUIRegistry instance;
    return instance;
}

void DebugUIRegistry::Clear() {
    items_.clear();
    groups_.clear();
    statusMessage_.clear();
}

void DebugUIRegistry::RegisterBool(const std::string &group,
                                   const std::string &label, bool *value,
                                   bool serializable) {
#ifdef _DEBUG
    DebugUIItem item{group, label, DebugUIValueType::Bool, value};
    item.serializable = serializable;
    AddItem(item);
#endif // _DEBUG
}

void DebugUIRegistry::RegisterInt(const std::string &group,
                                  const std::string &label, int *value,
                                  int minValue, int maxValue,
                                  bool serializable) {
    RegisterSliderInt(group, label, value, minValue, maxValue, serializable);
}

void DebugUIRegistry::RegisterSliderInt(const std::string &group,
                                        const std::string &label, int *value,
                                        int minValue, int maxValue,
                                        bool serializable) {
#ifdef _DEBUG
    DebugUIItem item{group, label, DebugUIValueType::Int, value};
    item.minInt = minValue;
    item.maxInt = maxValue;
    item.serializable = serializable;
    AddItem(item);
#endif // _DEBUG
}

void DebugUIRegistry::RegisterFloat(const std::string &group,
                                    const std::string &label, float *value,
                                    float minValue, float maxValue,
                                    bool serializable) {
    RegisterSliderFloat(group, label, value, minValue, maxValue, serializable);
}

void DebugUIRegistry::RegisterSliderFloat(const std::string &group,
                                          const std::string &label,
                                          float *value, float minValue,
                                          float maxValue, bool serializable) {
#ifdef _DEBUG
    DebugUIItem item{group, label, DebugUIValueType::Float, value};
    item.minFloat = minValue;
    item.maxFloat = maxValue;
    item.serializable = serializable;
    AddItem(item);
#endif // _DEBUG
}

void DebugUIRegistry::RegisterDragFloat(const std::string &group,
                                        const std::string &label, float *value,
                                        float speed, float minValue,
                                        float maxValue, bool serializable) {
#ifdef _DEBUG
    DebugUIItem item{group, label, DebugUIValueType::Float, value};
    item.minFloat = minValue;
    item.maxFloat = maxValue;
    item.speed = speed;
    item.control = DebugUIControlType::Drag;
    item.serializable = serializable;
    AddItem(item);
#endif // _DEBUG
}

void DebugUIRegistry::RegisterFloat2(const std::string &group,
                                     const std::string &label, float *value,
                                     float minValue, float maxValue,
                                     bool serializable) {
#ifdef _DEBUG
    DebugUIItem item{group, label, DebugUIValueType::Float2, value};
    item.minFloat = minValue;
    item.maxFloat = maxValue;
    item.serializable = serializable;
    AddItem(item);
#endif // _DEBUG
}

void DebugUIRegistry::RegisterFloat3(const std::string &group,
                                     const std::string &label, float *value,
                                     float minValue, float maxValue,
                                     bool serializable) {
#ifdef _DEBUG
    DebugUIItem item{group, label, DebugUIValueType::Float3, value};
    item.minFloat = minValue;
    item.maxFloat = maxValue;
    item.serializable = serializable;
    AddItem(item);
#endif // _DEBUG
}

void DebugUIRegistry::RegisterFloat4(const std::string &group,
                                     const std::string &label, float *value,
                                     float minValue, float maxValue,
                                     bool serializable) {
#ifdef _DEBUG
    DebugUIItem item{group, label, DebugUIValueType::Float4, value};
    item.minFloat = minValue;
    item.maxFloat = maxValue;
    item.serializable = serializable;
    AddItem(item);
#endif // _DEBUG
}

void DebugUIRegistry::RegisterDragFloat3(const std::string &group,
                                         const std::string &label,
                                         float *value, float speed,
                                         bool serializable) {
#ifdef _DEBUG
    DebugUIItem item{group, label, DebugUIValueType::Float3, value};
    item.speed = speed;
    item.control = DebugUIControlType::Drag;
    item.serializable = serializable;
    AddItem(item);
#endif // _DEBUG
}

void DebugUIRegistry::RegisterTransform(const std::string &group,
                                        const std::string &label,
                                        Transform *transform,
                                        bool serializable) {
#ifdef _DEBUG
    if (!transform) {
        return;
    }

    RegisterDragFloat3(group, label + " Position", &transform->position.x,
                       0.05f, serializable);
    RegisterDragFloat3(group, label + " Scale", &transform->scale.x, 0.01f,
                       serializable);
#endif // _DEBUG
}

void DebugUIRegistry::RegisterColor3(const std::string &group,
                                     const std::string &label, float *value,
                                     bool serializable) {
#ifdef _DEBUG
    DebugUIItem item{group, label, DebugUIValueType::Color3, value};
    item.serializable = serializable;
    AddItem(item);
#endif // _DEBUG
}

void DebugUIRegistry::RegisterColor4(const std::string &group,
                                     const std::string &label, float *value,
                                     bool serializable) {
#ifdef _DEBUG
    DebugUIItem item{group, label, DebugUIValueType::Color4, value};
    item.serializable = serializable;
    AddItem(item);
#endif // _DEBUG
}

void DebugUIRegistry::RegisterCombo(const std::string &group,
                                    const std::string &label, int *value,
                                    const std::vector<std::string> &labels,
                                    bool serializable) {
#ifdef _DEBUG
    DebugUIItem item{group, label, DebugUIValueType::Combo, value};
    item.comboLabels = labels;
    item.serializable = serializable;
    AddItem(item);
#endif // _DEBUG
}

void DebugUIRegistry::RegisterCombo(
    const std::string &group, const std::string &label, int *value,
    std::initializer_list<const char *> labels, bool serializable) {
#ifdef _DEBUG
    std::vector<std::string> stringLabels;
    stringLabels.reserve(labels.size());
    for (const char *labelText : labels) {
        stringLabels.emplace_back(labelText ? labelText : "");
    }
    RegisterCombo(group, label, value, stringLabels, serializable);
#endif // _DEBUG
}

void DebugUIRegistry::RegisterText(const std::string &group,
                                   const std::string &label,
                                   std::function<std::string()> getter) {
#ifdef _DEBUG
    DebugUIItem item{group, label, DebugUIValueType::Text};
    item.serializable = false;
    item.textGetter = std::move(getter);
    AddItem(item);
#endif // _DEBUG
}

void DebugUIRegistry::RegisterReadonlyFloat(const std::string &group,
                                            const std::string &label,
                                            std::function<float()> getter) {
#ifdef _DEBUG
    DebugUIItem item{group, label, DebugUIValueType::ReadonlyFloat};
    item.serializable = false;
    item.floatGetter = std::move(getter);
    AddItem(item);
#endif // _DEBUG
}

void DebugUIRegistry::RegisterReadonlyInt(const std::string &group,
                                          const std::string &label,
                                          std::function<int()> getter) {
#ifdef _DEBUG
    DebugUIItem item{group, label, DebugUIValueType::ReadonlyInt};
    item.serializable = false;
    item.intGetter = std::move(getter);
    AddItem(item);
#endif // _DEBUG
}

void DebugUIRegistry::SetVisible(bool visible) { visible_ = visible; }

bool DebugUIRegistry::IsVisible() const { return visible_; }

bool DebugUIRegistry::SaveToJson(const std::string &path) const {
#ifdef _DEBUG
    try {
        nlohmann::json root = nlohmann::json::object();
        for (const DebugUIItem &item : items_) {
            if (!item.serializable || !item.value) {
                continue;
            }

            switch (item.type) {
            case DebugUIValueType::Bool:
                root[item.group][item.label] =
                    *static_cast<const bool *>(item.value);
                break;
            case DebugUIValueType::Int:
            case DebugUIValueType::Combo:
                root[item.group][item.label] =
                    *static_cast<const int *>(item.value);
                break;
            case DebugUIValueType::Float:
                root[item.group][item.label] =
                    *static_cast<const float *>(item.value);
                break;
            case DebugUIValueType::Float2:
            case DebugUIValueType::Float3:
            case DebugUIValueType::Float4:
            case DebugUIValueType::Color3:
            case DebugUIValueType::Color4: {
                const int count = GetFloatComponentCount(item.type);
                const float *values = static_cast<const float *>(item.value);
                nlohmann::json array = nlohmann::json::array();
                for (int index = 0; index < count; ++index) {
                    array.push_back(values[index]);
                }
                root[item.group][item.label] = array;
                break;
            }
            default:
                break;
            }
        }

        const std::filesystem::path filePath(path);
        if (filePath.has_parent_path()) {
            std::filesystem::create_directories(filePath.parent_path());
        }

        std::ofstream file(filePath);
        if (!file) {
            statusMessage_ = "Save failed";
            return false;
        }

        file << root.dump(2);
        statusMessage_ = "Saved: " + path;
        return true;
    } catch (...) {
        statusMessage_ = "Save failed";
        return false;
    }
#else
    (void)path;
    return false;
#endif // _DEBUG
}

bool DebugUIRegistry::LoadFromJson(const std::string &path) {
#ifdef _DEBUG
    try {
        std::ifstream file(path);
        if (!file) {
            statusMessage_ = "Load failed";
            return false;
        }

        nlohmann::json root;
        file >> root;
        if (!root.is_object()) {
            statusMessage_ = "Load failed";
            return false;
        }

        for (DebugUIItem &item : items_) {
            if (!item.serializable || !item.value || !root.contains(item.group)) {
                continue;
            }

            const nlohmann::json &groupJson = root[item.group];
            if (!groupJson.is_object() || !groupJson.contains(item.label)) {
                continue;
            }

            const nlohmann::json &valueJson = groupJson[item.label];
            try {
                switch (item.type) {
                case DebugUIValueType::Bool:
                    if (valueJson.is_boolean()) {
                        *static_cast<bool *>(item.value) =
                            valueJson.get<bool>();
                    }
                    break;
                case DebugUIValueType::Int:
                case DebugUIValueType::Combo:
                    if (valueJson.is_number_integer()) {
                        *static_cast<int *>(item.value) =
                            valueJson.get<int>();
                    }
                    break;
                case DebugUIValueType::Float:
                    if (valueJson.is_number()) {
                        *static_cast<float *>(item.value) =
                            valueJson.get<float>();
                    }
                    break;
                case DebugUIValueType::Float2:
                case DebugUIValueType::Float3:
                case DebugUIValueType::Float4:
                case DebugUIValueType::Color3:
                case DebugUIValueType::Color4:
                    if (valueJson.is_array()) {
                        const int count = GetFloatComponentCount(item.type);
                        if (static_cast<int>(valueJson.size()) >= count) {
                            float *values = static_cast<float *>(item.value);
                            for (int index = 0; index < count; ++index) {
                                if (valueJson[index].is_number()) {
                                    values[index] =
                                        valueJson[index].get<float>();
                                }
                            }
                        }
                    }
                    break;
                default:
                    break;
                }
            } catch (...) {
                // 型不一致や変換失敗はその項目だけスキップする。
            }
        }

        statusMessage_ = "Loaded: " + path;
        return true;
    } catch (...) {
        statusMessage_ = "Load failed";
        return false;
    }
#else
    (void)path;
    return false;
#endif // _DEBUG
}

void DebugUIRegistry::ResetSerializableValues() {
#ifdef _DEBUG
    for (DebugUIItem &item : items_) {
        if (!item.serializable || !item.value) {
            continue;
        }

        switch (item.type) {
        case DebugUIValueType::Bool:
            *static_cast<bool *>(item.value) = item.initialBool;
            break;
        case DebugUIValueType::Int:
        case DebugUIValueType::Combo:
            *static_cast<int *>(item.value) = item.initialInt;
            break;
        case DebugUIValueType::Float:
            if (!item.initialFloats.empty()) {
                *static_cast<float *>(item.value) = item.initialFloats[0];
            }
            break;
        case DebugUIValueType::Float2:
        case DebugUIValueType::Float3:
        case DebugUIValueType::Float4:
        case DebugUIValueType::Color3:
        case DebugUIValueType::Color4: {
            const int count = GetFloatComponentCount(item.type);
            if (static_cast<int>(item.initialFloats.size()) >= count) {
                float *values = static_cast<float *>(item.value);
                for (int index = 0; index < count; ++index) {
                    values[index] = item.initialFloats[index];
                }
            }
            break;
        }
        default:
            break;
        }
    }
    statusMessage_ = "Reset serializable values";
#endif // _DEBUG
}

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
        // 将来的にはScene別ファイルやプリセット選択へ拡張する。
        if (ImGui::Button("Save")) {
            SaveToJson(kDefaultTuningPath);
        }
        ImGui::SameLine();
        if (ImGui::Button("Load")) {
            LoadFromJson(kDefaultTuningPath);
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            ResetSerializableValues();
        }
        ImGui::Text("Path: %s", kDefaultTuningPath);
        if (!statusMessage_.empty()) {
            ImGui::Text("%s", statusMessage_.c_str());
        }
        ImGui::Separator();

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
    CaptureInitialValue(item);

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

void DebugUIRegistry::CaptureInitialValue(DebugUIItem &item) const {
    if (!item.value) {
        return;
    }

    switch (item.type) {
    case DebugUIValueType::Bool:
        item.initialBool = *static_cast<bool *>(item.value);
        break;
    case DebugUIValueType::Int:
    case DebugUIValueType::Combo:
        item.initialInt = *static_cast<int *>(item.value);
        break;
    case DebugUIValueType::Float:
        item.initialFloats = {*static_cast<float *>(item.value)};
        break;
    case DebugUIValueType::Float2:
    case DebugUIValueType::Float3:
    case DebugUIValueType::Float4:
    case DebugUIValueType::Color3:
    case DebugUIValueType::Color4: {
        const int count = GetFloatComponentCount(item.type);
        const float *values = static_cast<const float *>(item.value);
        item.initialFloats.assign(values, values + count);
        break;
    }
    default:
        break;
    }
}

int DebugUIRegistry::GetFloatComponentCount(DebugUIValueType type) {
    switch (type) {
    case DebugUIValueType::Float2:
        return 2;
    case DebugUIValueType::Float3:
    case DebugUIValueType::Color3:
        return 3;
    case DebugUIValueType::Float4:
    case DebugUIValueType::Color4:
        return 4;
    default:
        return IsFloatArrayType(type) ? 1 : 0;
    }
}
