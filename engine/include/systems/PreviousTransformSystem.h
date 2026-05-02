#pragma once

class World;

/// <summary>
/// CCDや補間用に前フレームTransformを保存するSystem。
/// </summary>
struct PreviousTransformSystem {
    void Update(World &world);
};
