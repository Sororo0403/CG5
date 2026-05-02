#pragma once

class World;

/// <summary>
/// LocalTransformとParentComponentからWorld Transformを解決するSystem。
/// </summary>
struct TransformSystem {
    void Update(World &world);
};
