#include "gameobject/GameObject.h"

GameObject::GameObject(std::string name) : name_(std::move(name)) {
    transform_.OnAttach(*this);
}

void GameObject::Update(float deltaTime) {
    if (!active_) {
        return;
    }

    for (auto &component : components_) {
        component->Update(deltaTime);
    }
}

void GameObject::Draw(const Camera &camera) {
    if (!active_) {
        return;
    }

    for (auto &component : components_) {
        component->Draw(camera);
    }
}
