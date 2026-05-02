#pragma once
#include <DirectXMath.h>

/// <summary>
/// 物理更新に必要な汎用パラメータを保持するComponent。
/// </summary>
struct PhysicsBody {
    DirectX::XMFLOAT3 acceleration{0.0f, 0.0f, 0.0f};
    bool useGravity = false;
    float gravityScale = 1.0f;
};
