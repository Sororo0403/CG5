#include "Inspector.h"
#include "Camera.h"
#include "EngineRuntime.h"
#include "Transform.h"
#include <algorithm>
#include <utility>

#ifdef _DEBUG
#include "imgui.h"
#endif // _DEBUG

Inspector &Inspector::GetInstance() {
    static Inspector instance;
    return instance;
}

void Inspector::Clear() {
    objects_.clear();
    selectedIndex_ = -1;
}

void Inspector::RegisterObject(const std::string &name, void *object,
                               std::function<void()> drawFunc) {
    RegisterObject("Objects", name, object, std::move(drawFunc));
}

void Inspector::RegisterObject(const std::string &category,
                               const std::string &name, void *object,
                               std::function<void()> drawFunc) {
#ifdef _DEBUG
    InspectorObject item{category, name, object, std::move(drawFunc)};

    auto existing = std::find_if(
        objects_.begin(), objects_.end(), [&item](const InspectorObject &obj) {
            return obj.category == item.category && obj.name == item.name;
        });
    if (existing != objects_.end()) {
        *existing = std::move(item);
        return;
    }

    objects_.push_back(std::move(item));
    if (selectedIndex_ < 0) {
        selectedIndex_ = 0;
    }
#endif // _DEBUG
}

void Inspector::RegisterTransform(const std::string &name,
                                  Transform *transform) {
#ifdef _DEBUG
    if (!transform) {
        return;
    }

    RegisterObject("Objects", name, transform, [name, transform]() {
        DrawTransform(name, *transform);
    });
#endif // _DEBUG
}

void Inspector::RegisterCameraInfo(const std::string &name,
                                   const Camera *camera) {
#ifdef _DEBUG
    if (!camera) {
        return;
    }

    RegisterObject("Objects", name, nullptr, [camera]() {
                       const DirectX::XMFLOAT3 &position =
                           camera->GetPosition();
                       const DirectX::XMFLOAT3 &rotation =
                           camera->GetRotation();
                       const DirectX::XMFLOAT3 &target = camera->GetTarget();

                       ImGui::Text("Camera");
                       ImGui::Separator();
                       ImGui::Text("Position: %.3f, %.3f, %.3f", position.x,
                                   position.y, position.z);
                       ImGui::Text("Rotation: %.3f, %.3f, %.3f", rotation.x,
                                   rotation.y, rotation.z);
                       ImGui::Text("Target: %.3f, %.3f, %.3f", target.x,
                                   target.y, target.z);
                       ImGui::Text("Near / Far: %.3f / %.3f",
                                   camera->GetNearZ(), camera->GetFarZ());
                       // カメラ編集はsetter連動が必要なため、まずは情報表示に留める。
                   });
#endif // _DEBUG
}

void Inspector::SetVisible(bool visible) { visible_ = visible; }

bool Inspector::IsVisible() const { return visible_; }

void Inspector::Draw() {
#ifdef _DEBUG
    const EngineRuntime &runtime = EngineRuntime::GetInstance();
    if (!runtime.IsTuningMode() || !runtime.Settings().showDebugUI ||
        !visible_) {
        return;
    }

    if (objects_.empty()) {
        return;
    }

    if (selectedIndex_ < 0 ||
        selectedIndex_ >= static_cast<int>(objects_.size())) {
        selectedIndex_ = 0;
    }

    if (ImGui::Begin("Inspector")) {
        ImGui::Text("Objects");
        ImGui::Separator();

        std::string currentCategory;
        for (int index = 0; index < static_cast<int>(objects_.size());
             ++index) {
            const InspectorObject &object = objects_[index];
            if (object.category != currentCategory) {
                currentCategory = object.category;
                ImGui::Text("%s", currentCategory.c_str());
            }

            const bool selected = selectedIndex_ == index;
            if (ImGui::Selectable(object.name.c_str(), selected)) {
                selectedIndex_ = index;
            }
        }

        ImGui::Separator();
        const InspectorObject &selectedObject = objects_[selectedIndex_];
        ImGui::Text("Selected: %s", selectedObject.name.c_str());
        ImGui::Separator();

        if (selectedObject.drawFunc) {
            selectedObject.drawFunc();
        }
    }
    ImGui::End();
#endif // _DEBUG
}

void Inspector::DrawTransform(const std::string &label,
                              Transform &transform) {
#ifdef _DEBUG
    if (ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Position", &transform.position.x, 0.05f);
        // rotationはQuaternionのため、ここでは直接編集しない。
        ImGui::DragFloat3("Scale", &transform.scale.x, 0.01f);
    }
#else
    (void)label;
    (void)transform;
#endif // _DEBUG
}
