#pragma once

class World;

/// <summary>
/// 汎用AI ComponentをVelocityへ反映するSystem。
/// </summary>
struct AISystem {
    void Update(World &world, float deltaTime);
};
