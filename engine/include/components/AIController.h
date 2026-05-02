#pragma once
#include "Entity.h"

/// <summary>
/// 対象を追跡する簡易AI。
/// </summary>
struct PursuitAI {
    Entity target = kInvalidEntity;
    float moveSpeed = 4.0f;
    float acceleration = 18.0f;
    float stopDistance = 1.5f;
    float aggroDistance = 14.0f;
};
