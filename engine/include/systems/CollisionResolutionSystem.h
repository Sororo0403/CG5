#pragma once

class World;

/// <summary>
/// SolidなCollider同士の重なりを押し戻して解決するSystem。
/// </summary>
struct CollisionResolutionSystem {
    void Update(World &world);
};
