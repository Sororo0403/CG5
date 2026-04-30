#pragma once
#include "IEditableObject.h"
#include <cstddef>
#include <string>

class IEditableScene {
  public:
    virtual ~IEditableScene() = default;

    virtual size_t GetEditableObjectCount() const = 0;
    virtual IEditableObject *GetEditableObject(size_t index) = 0;
    virtual const IEditableObject *GetEditableObject(size_t index) const = 0;

    virtual int GetSelectedEditableObjectIndex() const = 0;
    virtual void SetSelectedEditableObjectIndex(int index) = 0;
    virtual void OnEditableObjectChanged(size_t index) = 0;

    virtual bool SaveScene(std::string *message) = 0;
    virtual bool LoadScene(std::string *message) = 0;

    virtual void OnEnterEditorMode() {}
    virtual void OnEnterGameplayMode() {}
    virtual void SetGameplayPaused(bool paused) { (void)paused; }
    virtual bool IsGameplayPaused() const { return false; }
    virtual void ResetGameplay() {}
};
