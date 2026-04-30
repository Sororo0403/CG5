#pragma once
#include "EditorConsole.h"
#include "EditorContext.h"
#include "panels/ConsolePanel.h"
#include "panels/HierarchyPanel.h"
#include "panels/InspectorPanel.h"

class IEditableScene;

class EditorLayer {
  public:
    void Draw(IEditableScene *scene);

  private:
    void DrawToolbar(EditorContext &context);

    EditorConsole console_;
    HierarchyPanel hierarchyPanel_;
    InspectorPanel inspectorPanel_;
    ConsolePanel consolePanel_;
};
