#pragma once
#include <functional>
#include <string>
#include <vector>

class IEditableObject;
class IEditableScene;

class EditableObjectFactory {
  public:
    using CreateFunc = std::function<IEditableObject *(IEditableScene &)>;

    static EditableObjectFactory &GetInstance();

    void Register(const std::string &type, CreateFunc func);
    IEditableObject *Create(const std::string &type, IEditableScene &scene) const;
    const std::vector<std::string> &GetRegisteredTypes() const;

  private:
    struct Entry {
        std::string type;
        CreateFunc create;
    };

    std::vector<Entry> entries_;
    std::vector<std::string> registeredTypes_;
};
