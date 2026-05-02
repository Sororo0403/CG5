#pragma once

class World;

/// <summary>
/// 足元の地面へ接地スナップするSystem。
/// </summary>
struct GroundingSystem {
    void Update(World &world);
};
