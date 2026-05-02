#pragma once

class World;

/// <summary>
/// AnimationStateMachineの要求をAnimationComponentへ反映するSystem。
/// </summary>
struct AnimationStateMachineSystem {
    void Update(World &world, float deltaTime);
};
