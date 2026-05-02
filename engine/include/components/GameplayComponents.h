#pragma once
#include "Entity.h"

#include <DirectXMath.h>
#include <cstdint>

/// <summary>
/// HPと無敵時間を保持するComponent。
/// </summary>
struct Health {
    int current = 1;
    int maximum = 1;
    float invulnerabilityTime = 0.0f;
};

/// <summary>
/// 所属チームと攻撃対象マスク。
/// </summary>
struct Faction {
    uint32_t layer = 1;
    uint32_t mask = 0xffffffff;
};

/// <summary>
/// ダメージを与える攻撃判定。
/// </summary>
struct Hitbox {
    int damage = 1;
    float hitStop = 0.0f;
    DirectX::XMFLOAT3 knockback{0.0f, 0.0f, 0.0f};
    Entity owner = kInvalidEntity;
    bool active = true;
    float victimInvulnerabilityTime = 0.2f;
};

/// <summary>
/// 攻撃判定の寿命。
/// </summary>
struct HitboxLifetime {
    float timeRemaining = 0.1f;
    bool destroyEntityWhenExpired = false;
};

/// <summary>
/// ダメージを受ける判定。
/// </summary>
struct Hurtbox {
    Entity owner = kInvalidEntity;
    bool active = true;
};

/// <summary>
/// 1回のダメージ成立を表すイベント。
/// </summary>
struct DamageEvent {
    Entity attacker = kInvalidEntity;
    Entity victim = kInvalidEntity;
    int damage = 0;
    DirectX::XMFLOAT3 knockback{0.0f, 0.0f, 0.0f};
};
