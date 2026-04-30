#pragma once
#include <cstdint>

struct EditorContext;
struct EditableObjectDesc;
class IEditableObject;
class IEditableScene;

class InspectorPanel {
  public:
    void Draw(EditorContext &context);

  private:
    void SyncNameBuffer(const EditableObjectDesc &desc);
    void CommitName(IEditableScene &scene, IEditableObject &object,
                    size_t selectedIndex);

    char nameBuffer_[128]{};
    uint64_t editingObjectId_ = 0;
};
