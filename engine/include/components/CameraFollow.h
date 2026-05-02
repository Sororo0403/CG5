#pragma once
#include "Entity.h"

#include <DirectXMath.h>

/// <summary>
/// カメラが追従する対象とオフセット。
/// </summary>
struct CameraFollow {
    Entity target = kInvalidEntity;
    DirectX::XMFLOAT3 offset{0.0f, 4.0f, -8.0f};
    DirectX::XMFLOAT3 lookOffset{0.0f, 1.4f, 0.0f};
    float positionSharpness = 12.0f;
    float lookSharpness = 16.0f;
};
