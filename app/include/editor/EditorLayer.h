#pragma once
#include <string>
#include <vector>

class GridPlacementTest;

class EditorLayer {
  public:
    void Draw(GridPlacementTest &placement);

    bool IsPaused() const { return paused_; }

  private:
    void DrawToolbar(GridPlacementTest &placement);
    void DrawHierarchy(GridPlacementTest &placement);
    void DrawInspector(GridPlacementTest &placement);
    void DrawConsole();
    void AddLog(const std::string &message);

    bool paused_ = false;
    std::vector<std::string> logs_;
};
