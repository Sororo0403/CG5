#pragma once

class World;

/// <summary>
/// 高速落下時の縦方向すり抜けを補正する簡易CCD。
/// </summary>
struct ContinuousCollisionSystem {
    void Update(World &world);
};
