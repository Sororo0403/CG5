#pragma once
#include "Entity.h"

#include <DirectXMath.h>

/// <summary>
/// アクションゲーム向けの移動パラメータ。
/// </summary>
struct CharacterController {
    float moveSpeed = 6.0f;
    float groundAcceleration = 45.0f;
    float airAcceleration = 18.0f;
    float jumpSpeed = 8.5f;
    float coyoteTime = 0.10f;
    float dashSpeed = 14.0f;
    float dashDuration = 0.16f;
    float dashCooldown = 0.35f;
    float dashTimeRemaining = 0.0f;
    float dashCooldownRemaining = 0.0f;
    bool faceMoveDirection = true;
};

/// <summary>
/// 直近の接地状態。
/// </summary>
struct GroundedState {
    bool grounded = false;
    Entity groundEntity = kInvalidEntity;
    DirectX::XMFLOAT3 normal{0.0f, 1.0f, 0.0f};
    float timeSinceGrounded = 999.0f;
};

/// <summary>
/// 入力より優先して適用するノックバック。
/// </summary>
struct Knockback {
    DirectX::XMFLOAT3 velocity{0.0f, 0.0f, 0.0f};
    float timeRemaining = 0.0f;
    bool overrideInput = true;
};
