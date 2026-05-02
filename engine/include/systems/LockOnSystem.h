#pragma once

class World;

/// <summary>
/// 入力に応じて近いLockOnTargetを選択・解除するSystem。
/// </summary>
struct LockOnSystem {
    void Update(World &world);
};
