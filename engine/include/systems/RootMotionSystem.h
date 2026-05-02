#pragma once

class World;

/// <summary>
/// SkeletonPoseComponentのroot motion差分をTransformへ適用するSystem。
/// </summary>
struct RootMotionSystem {
    void Update(World &world);
};
