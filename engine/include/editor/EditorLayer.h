#pragma once
#include "EditorConsole.h"
#include "EditorContext.h"
#include "panels/ConsolePanel.h"
#include "panels/HierarchyPanel.h"
#include "panels/InspectorPanel.h"
#include "panels/ViewportPanel.h"

class IEditableScene;
class RenderTexture;

class EditorLayer {
  public:
    void Draw(IEditableScene *scene, RenderTexture *renderTexture);

  private:
    void DrawToolbar(EditorContext &context);
    void ApplyViewportInputState(const EditorContext &context);

    EditorConsole console_;
    HierarchyPanel hierarchyPanel_;
    ViewportPanel viewportPanel_;
    InspectorPanel inspectorPanel_;
    ConsolePanel consolePanel_;
};
