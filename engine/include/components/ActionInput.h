#pragma once
#include <DirectXMath.h>

/// <summary>
/// アクションゲーム向けに正規化された入力状態。
/// </summary>
struct ActionInput {
    DirectX::XMFLOAT2 move{0.0f, 0.0f};
    DirectX::XMFLOAT2 look{0.0f, 0.0f};
    bool jumpPressed = false;
    bool jumpHeld = false;
    bool attackPressed = false;
    bool guardHeld = false;
    bool dashPressed = false;
    bool lockOnPressed = false;
};

/// <summary>
/// 先行入力を短時間保持するComponent。
/// </summary>
struct InputBuffer {
    float jumpTime = 0.0f;
    float attackTime = 0.0f;
    float dashTime = 0.0f;
    float bufferDuration = 0.12f;
};
