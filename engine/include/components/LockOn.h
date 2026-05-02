#pragma once
#include "Entity.h"

/// <summary>
/// ロックオン対象として選択されるComponent。
/// </summary>
struct LockOnTarget {
    float priority = 1.0f;
    bool enabled = true;
};

/// <summary>
/// ロックオンの状態と探索パラメータ。
/// </summary>
struct LockOnState {
    Entity target = kInvalidEntity;
    float maxDistance = 18.0f;
    float releaseDistance = 24.0f;
    bool locked = false;
};
