#include "editor/EditorLayer.h"
#include "EngineRuntime.h"
#include "GridPlacementTest.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {

constexpr float kToolbarHeight = 42.0f;
constexpr float kHierarchyWidth = 260.0f;
constexpr float kInspectorWidth = 320.0f;
constexpr float kConsoleHeight = 170.0f;

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

void EditorLayer::Draw(GridPlacementTest &placement) {
#ifdef _DEBUG
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    const ImVec2 origin = viewport->WorkPos;
    const ImVec2 size = viewport->WorkSize;

    ImGui::SetNextWindowPos(origin);
    ImGui::SetNextWindowSize({size.x, kToolbarHeight});
    DrawToolbar(placement);

    ImGui::SetNextWindowPos({origin.x, origin.y + kToolbarHeight});
    ImGui::SetNextWindowSize(
        {kHierarchyWidth, (std::max)(0.0f, size.y - kToolbarHeight - kConsoleHeight)});
    DrawHierarchy(placement);

    ImGui::SetNextWindowPos(
        {origin.x + size.x - kInspectorWidth, origin.y + kToolbarHeight});
    ImGui::SetNextWindowSize(
        {kInspectorWidth, (std::max)(0.0f, size.y - kToolbarHeight - kConsoleHeight)});
    DrawInspector(placement);

    ImGui::SetNextWindowPos({origin.x, origin.y + size.y - kConsoleHeight});
    ImGui::SetNextWindowSize({size.x, kConsoleHeight});
    DrawConsole();
#else
    (void)placement;
#endif // _DEBUG
}

void EditorLayer::DrawToolbar(GridPlacementTest &placement) {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoTitleBar;
    if (!ImGui::Begin("Editor Toolbar", nullptr, flags)) {
        ImGui::End();
        return;
    }

    EngineRuntime &runtime = EngineRuntime::GetInstance();
    const bool engineMode = runtime.IsTuningMode();
    if (ImGui::Button(engineMode ? "Engine Mode" : "Gameplay Mode")) {
        runtime.SetMode(engineMode ? EngineRuntimeMode::Play
                                   : EngineRuntimeMode::Tuning);
        AddLog(runtime.IsTuningMode() ? "Switched to Engine Mode"
                                      : "Switched to Gameplay Mode");
    }

    ImGui::SameLine();
    if (ImGui::Button(paused_ ? "Play" : "Pause")) {
        paused_ = !paused_;
        AddLog(paused_ ? "Paused" : "Playing");
    }

    ImGui::SameLine();
    if (ImGui::Button("Save Scene")) {
        AddLog(placement.SaveStage() ? "Saved scene: " + placement.GetStagePath()
                                     : "Failed to save scene: " +
                                           placement.GetStagePath());
    }

    ImGui::SameLine();
    if (ImGui::Button("Load Scene")) {
        AddLog(placement.LoadStage() ? "Loaded scene: " + placement.GetStagePath()
                                     : "Failed to load scene: " +
                                           placement.GetStagePath());
    }

    ImGui::SameLine();
    ImGui::Text("Objects: %zu", placement.GetObjectCount());
    ImGui::End();
}

void EditorLayer::DrawHierarchy(GridPlacementTest &placement) {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoCollapse;
    if (!ImGui::Begin("Hierarchy", nullptr, flags)) {
        ImGui::End();
        return;
    }

    for (int index = 0; index < static_cast<int>(placement.GetObjectCount());
         ++index) {
        const PlacementObject *object =
            placement.GetPlacementObject(static_cast<size_t>(index));
        if (!object) {
            continue;
        }

        char label[128]{};
        std::snprintf(label, sizeof(label), "%03d  %s", index,
                      object->name.c_str());
        if (ImGui::Selectable(label, placement.GetSelectedIndex() == index)) {
            placement.SetSelectedIndex(index);
        }
    }

    ImGui::End();
}

void EditorLayer::DrawInspector(GridPlacementTest &placement) {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoCollapse;
    if (!ImGui::Begin("Inspector", nullptr, flags)) {
        ImGui::End();
        return;
    }

    PlacementObject *object =
        placement.GetPlacementObject(static_cast<size_t>(placement.GetSelectedIndex()));
    if (!object) {
        ImGui::Text("No object selected");
        ImGui::End();
        return;
    }

    char nameBuffer[128]{};
    std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", object->name.c_str());
    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
        object->name = nameBuffer;
    }

    ImGui::Text("Type: %s", GridPlacementTest::GetKindName(object->kind));
    ImGui::Text("Grid: %d, %d", object->gridX, object->gridY);
    ImGui::Separator();

    bool changed = false;
    changed |=
        ImGui::DragFloat3("Position", &object->transform.position.x, 0.05f);
    changed |=
        ImGui::DragFloat4("Rotation", &object->transform.rotation.x, 0.005f);
    changed |= ImGui::DragFloat3("Scale", &object->transform.scale.x, 0.02f);
    if (changed) {
        NormalizeQuaternion(object->transform.rotation);
        placement.OnEditorObjectChanged(
            static_cast<size_t>(placement.GetSelectedIndex()));
    }

    ImGui::Separator();
    if (object->kind == PlacementObjectKind::Wall) {
        ImGui::Text("Collider: blocked grid tile");
    } else if (object->kind == PlacementObjectKind::Floor ||
               object->kind == PlacementObjectKind::PlayerStart ||
               object->kind == PlacementObjectKind::EnemyMarker) {
        ImGui::Text("Collider: walkable grid tile");
    } else {
        ImGui::Text("Collider: none");
    }

    ImGui::End();
}

void EditorLayer::DrawConsole() {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoCollapse;
    if (!ImGui::Begin("Console / Log", nullptr, flags)) {
        ImGui::End();
        return;
    }

    if (logs_.empty()) {
        ImGui::Text("Ready");
    } else {
        for (const std::string &log : logs_) {
            ImGui::TextUnformatted(log.c_str());
        }
    }

    ImGui::End();
}

void EditorLayer::AddLog(const std::string &message) {
    logs_.push_back(message);
    if (logs_.size() > 80) {
        logs_.erase(logs_.begin(), logs_.begin() + (logs_.size() - 80));
    }
}
