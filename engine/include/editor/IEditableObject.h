#pragma once
#include <DirectXMath.h>
#include <string>

struct EditableTransform {
    DirectX::XMFLOAT3 position{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT4 rotation{0.0f, 0.0f, 0.0f, 1.0f};
    DirectX::XMFLOAT3 scale{1.0f, 1.0f, 1.0f};
};

struct EditableObjectDesc {
    std::string name;
    std::string type;
    std::string collider;
};

class IEditableObject {
  public:
    virtual ~IEditableObject() = default;

    virtual EditableObjectDesc GetEditorDesc() const = 0;
    virtual void SetEditorName(const std::string &name) = 0;
    virtual EditableTransform GetEditorTransform() const = 0;
    virtual void SetEditorTransform(const EditableTransform &transform) = 0;
};
