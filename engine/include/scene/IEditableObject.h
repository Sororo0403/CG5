#pragma once
#include <DirectXMath.h>
#include <cstdint>
#include <string>

struct EditableTransform {
    DirectX::XMFLOAT3 position{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT4 rotation{0.0f, 0.0f, 0.0f, 1.0f};
    DirectX::XMFLOAT3 scale{1.0f, 1.0f, 1.0f};
};

struct EditableRay {
    DirectX::XMFLOAT3 origin{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 direction{0.0f, 0.0f, 1.0f};
};

struct EditableObjectDesc {
    uint64_t id = 0;
    std::string name;
    std::string type;
    std::string collider;
    bool editable = true;
    bool locked = false;
    bool visible = true;
    std::string warning;
};

class IEditableObject {
  public:
    virtual ~IEditableObject() = default;

    virtual EditableObjectDesc GetEditorDesc() const = 0;
    virtual void SetEditorName(const std::string &name) = 0;
    virtual bool SetEditorCollider(const std::string &collider) {
        (void)collider;
        return false;
    }
    virtual bool SetEditorLocked(bool locked) {
        (void)locked;
        return false;
    }
    virtual bool SetEditorVisible(bool visible) {
        (void)visible;
        return false;
    }
    virtual EditableTransform GetEditorTransform() const = 0;
    virtual void SetEditorTransform(const EditableTransform &transform) = 0;
};
