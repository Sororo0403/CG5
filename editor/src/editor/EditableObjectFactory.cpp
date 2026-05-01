#include "EditableObjectFactory.h"
#include <algorithm>
#include <utility>

EditableObjectFactory &EditableObjectFactory::GetInstance() {
    static EditableObjectFactory instance;
    return instance;
}

void EditableObjectFactory::Register(const std::string &type, CreateFunc func) {
    if (type.empty() || !func) {
        return;
    }

    auto it = std::find_if(entries_.begin(), entries_.end(),
                           [&type](const Entry &entry) {
                               return entry.type == type;
                           });
    if (it != entries_.end()) {
        it->create = std::move(func);
        return;
    }

    entries_.push_back({type, std::move(func)});
    registeredTypes_.push_back(type);
}

IEditableObject *EditableObjectFactory::Create(const std::string &type,
                                               IEditableScene &scene) const {
    auto it = std::find_if(entries_.begin(), entries_.end(),
                           [&type](const Entry &entry) {
                               return entry.type == type;
                           });
    if (it == entries_.end() || !it->create) {
        return nullptr;
    }
    return it->create(scene);
}

const std::vector<std::string> &EditableObjectFactory::GetRegisteredTypes()
    const {
    return registeredTypes_;
}
