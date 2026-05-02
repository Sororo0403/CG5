#pragma once

class Camera;
class GameObject;

class Component {
  public:
    virtual ~Component() = default;

    virtual void OnAttach(GameObject &owner) { owner_ = &owner; }
    virtual void Update(float deltaTime) { (void)deltaTime; }
    virtual void Draw(const Camera &camera) { (void)camera; }

    GameObject *GetOwner() { return owner_; }
    const GameObject *GetOwner() const { return owner_; }

  private:
    GameObject *owner_ = nullptr;
};
