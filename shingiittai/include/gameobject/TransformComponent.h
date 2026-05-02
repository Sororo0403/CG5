#pragma once
#include "Component.h"
#include "Transform.h"

class TransformComponent : public Component {
  public:
    Transform &GetTransform() { return transform_; }
    const Transform &GetTransform() const { return transform_; }
    void SetTransform(const Transform &transform) { transform_ = transform; }

    DirectX::XMFLOAT3 &Position() { return transform_.position; }
    DirectX::XMFLOAT4 &Rotation() { return transform_.rotation; }
    DirectX::XMFLOAT3 &Scale() { return transform_.scale; }

  private:
    Transform transform_{};
};
