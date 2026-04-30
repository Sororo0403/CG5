#pragma once
#include <cstdint>

struct EditorContext;
struct EditableObjectDesc;
class IEditableObject;
class IEditableScene;

class HierarchyPanel {
  public:
    void Draw(EditorContext &context);

  private:
    void BeginRename(const EditableObjectDesc &desc);
    bool DrawObjectRow(EditorContext &context, IEditableScene &scene,
                       IEditableObject &object, size_t index);

    char searchBuffer_[128]{};
    char renameBuffer_[128]{};
    uint64_t renamingObjectId_ = 0;
    bool renameFocusPending_ = false;
};
