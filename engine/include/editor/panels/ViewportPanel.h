#pragma once
#include "imgui.h"
#include <string>

struct EditorContext;

class ViewportPanel {
  public:
    void Draw(EditorContext &context);

    const ImVec2 &GetPosition() const { return position_; }
    const ImVec2 &GetSize() const { return size_; }
    bool IsHovered() const { return hovered_; }
    bool IsFocused() const { return focused_; }

  private:
    void DrawGizmo(EditorContext &context);

    ImVec2 position_{0.0f, 0.0f};
    ImVec2 size_{0.0f, 0.0f};
    bool hovered_ = false;
    bool focused_ = false;
    int gizmoOperation_ = 0;
    int gizmoMode_ = 1;
    bool gizmoEditActive_ = false;
    std::string gizmoBeforeState_;
};
