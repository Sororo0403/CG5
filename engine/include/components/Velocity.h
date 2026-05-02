#pragma once
#include <DirectXMath.h>

/// <summary>
/// Entityの線形速度を保持するComponent。
/// </summary>
struct Velocity {
    DirectX::XMFLOAT3 linear{0.0f, 0.0f, 0.0f};
};
