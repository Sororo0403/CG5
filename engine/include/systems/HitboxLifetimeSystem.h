#pragma once

class World;
class WorldCommandBuffer;

/// <summary>
/// 時間制限付きHitboxを更新するSystem。
/// </summary>
struct HitboxLifetimeSystem {
    void Update(World &world, WorldCommandBuffer &commands, float deltaTime);
};
