#pragma once
#include "Entity.h"
#include "Transform.h"

#include <DirectXMath.h>

/// <summary>
/// 親Entityを参照するComponent。
/// </summary>
struct ParentComponent {
    Entity parent = kInvalidEntity;
};

/// <summary>
/// 親からの相対Transformを保持するComponent。
/// </summary>
struct LocalTransform {
    Transform transform{};
};

/// <summary>
/// 親子階層を解決したWorld行列を保持するComponent。
/// </summary>
struct WorldTransformMatrix {
    DirectX::XMFLOAT4X4 matrix = {
        1.0f, 0.0f, 0.0f, 0.0f, //
        0.0f, 1.0f, 0.0f, 0.0f, //
        0.0f, 0.0f, 1.0f, 0.0f, //
        0.0f, 0.0f, 0.0f, 1.0f  //
    };
};
