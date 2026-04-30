#pragma once
#include <cstdint>
#include <string>

struct EditorContext;
struct EditableObjectDesc;
class IEditableObject;
class IEditableScene;

class InspectorPanel {
  public:
    void Draw(EditorContext &context);

  private:
    void SyncNameBuffer(const EditableObjectDesc &desc);
    void CommitName(EditorContext &context, IEditableScene &scene,
                    IEditableObject &object, size_t selectedIndex);

    char nameBuffer_[128]{};
    uint64_t editingObjectId_ = 0;
    bool transformEditActive_ = false;
    std::string transformBeforeState_;
};
