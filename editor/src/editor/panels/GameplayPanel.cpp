#include "panels/GameplayPanel.h"
#include "EditorContext.h"
#include "imgui.h"

void GameplayPanel::Draw(EditorContext &context) {
#ifdef _DEBUG
    if (!ImGui::Begin("Gameplay Editor")) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("GameplayEditorTabs")) {
        if (ImGui::BeginTabItem("Enemy")) {
            enemyTuningPanel_.Draw(context);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
#else
    (void)context;
#endif
}
