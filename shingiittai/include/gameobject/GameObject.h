#pragma once
#include "TransformComponent.h"

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

class GameObject {
  public:
    explicit GameObject(std::string name = {});
    ~GameObject() = default;

    GameObject(const GameObject &) = delete;
    GameObject &operator=(const GameObject &) = delete;
    GameObject(GameObject &&) = delete;
    GameObject &operator=(GameObject &&) = delete;

    const std::string &GetName() const { return name_; }
    void SetActive(bool active) { active_ = active; }
    bool IsActive() const { return active_; }

    TransformComponent &GetTransformComponent() { return transform_; }
    const TransformComponent &GetTransformComponent() const { return transform_; }
    Transform &GetTransform() { return transform_.GetTransform(); }
    const Transform &GetTransform() const { return transform_.GetTransform(); }

    template <class T, class... Args> T &AddComponent(Args &&...args) {
        static_assert(std::is_base_of_v<Component, T>,
                      "T must derive from Component");
        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        T &ref = *component;
        component->OnAttach(*this);
        components_.push_back(std::move(component));
        return ref;
    }

    template <class T> T *GetComponent() {
        if constexpr (std::is_same_v<T, TransformComponent>) {
            return &transform_;
        } else {
            for (auto &component : components_) {
                if (auto *typed = dynamic_cast<T *>(component.get())) {
                    return typed;
                }
            }
            return nullptr;
        }
    }

    template <class T> const T *GetComponent() const {
        if constexpr (std::is_same_v<T, TransformComponent>) {
            return &transform_;
        } else {
            for (const auto &component : components_) {
                if (auto *typed = dynamic_cast<const T *>(component.get())) {
                    return typed;
                }
            }
            return nullptr;
        }
    }

    void Update(float deltaTime);
    void Draw(const Camera &camera);

  private:
    std::string name_{};
    bool active_ = true;
    TransformComponent transform_{};
    std::vector<std::unique_ptr<Component>> components_{};
};
