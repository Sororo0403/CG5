#pragma once
#include "Transform.h"

#include <DirectXMath.h>

/// <summary>
/// 接地を補助するためのスナップ/段差設定。
/// </summary>
struct GroundingAssist {
    float snapDistance = 0.18f;
    float stepHeight = 0.35f;
    float skinWidth = 0.02f;
};

/// <summary>
/// 連続衝突判定用に前フレームTransformを保持するComponent。
/// </summary>
struct PreviousTransform {
    Transform transform{};
};

/// <summary>
/// 地面として扱う法線。
/// </summary>
struct GroundSurface {
    DirectX::XMFLOAT3 normal{0.0f, 1.0f, 0.0f};
};
