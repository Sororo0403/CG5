#pragma once
#include "IEditableObject.h"
#include <cstddef>
#include <cstdint>
#include <string>

class IEditableScene {
  public:
    virtual ~IEditableScene() = default;

    virtual size_t GetEditableObjectCount() const = 0;
    virtual IEditableObject *GetEditableObject(size_t index) = 0;
    virtual const IEditableObject *GetEditableObject(size_t index) const = 0;

    virtual uint64_t GetSelectedObjectId() const = 0;
    virtual void SetSelectedObjectById(uint64_t id) = 0;

    virtual int GetSelectedEditableObjectIndex() const = 0;
    virtual void SetSelectedEditableObjectIndex(int index) = 0;
    virtual void OnEditableObjectChanged(size_t index) = 0;

    virtual bool SaveScene(std::string *message) = 0;
    virtual bool LoadScene(std::string *message) = 0;
    virtual bool SaveSceneAs(const std::string &path, std::string *message) {
        (void)path;
        return SaveScene(message);
    }
    virtual bool LoadSceneFromPath(const std::string &path,
                                   std::string *message) {
        (void)path;
        return LoadScene(message);
    }
    virtual std::string GetCurrentScenePath() const { return {}; }
    virtual std::string GetCurrentSceneName() const { return {}; }

    virtual bool IsSceneDirty() const { return false; }
    virtual void MarkSceneDirty() {}
    virtual void ClearSceneDirty() {}

    virtual bool CanEditObjects() const { return false; }
    virtual bool AddEditableObject(const std::string &type,
                                   std::string *message) {
        (void)type;
        if (message) {
            *message = "Object editing is not supported";
        }
        return false;
    }
    virtual bool DeleteSelectedEditableObject(std::string *message) {
        if (message) {
            *message = "Object editing is not supported";
        }
        return false;
    }
    virtual bool DuplicateSelectedEditableObject(std::string *message) {
        if (message) {
            *message = "Object editing is not supported";
        }
        return false;
    }

    virtual void OnEnterEditorMode() {}
    virtual void OnEnterGameplayMode() {}
    virtual void SetGameplayPaused(bool paused) { (void)paused; }
    virtual bool IsGameplayPaused() const { return false; }
    virtual void ResetGameplay() {}
};
