#pragma once
#include <DirectXMath.h>
#include <cstdint>

/// <summary>
/// Colliderの形状種別。
/// </summary>
enum class ColliderShape {
    AABB,
    OBB,
    Sphere,
    Capsule,
};

/// <summary>
/// 衝突判定に必要な形状とレイヤー情報を保持するComponent。
/// </summary>
struct Collider {
    ColliderShape shape = ColliderShape::AABB;
    DirectX::XMFLOAT3 center{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 halfSize{0.5f, 0.5f, 0.5f};
    float radius = 0.5f;
    uint32_t layer = 1;
    uint32_t mask = 0xffffffff;
    bool isTrigger = false;
    float capsuleHalfHeight = 0.5f;
};
