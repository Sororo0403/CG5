#pragma once
#include "Animator.h"

class ModelAssets;
class World;

/// <summary>
/// AnimationComponentをEntityごとのSkeletonPoseComponentへ反映するSystem。
/// </summary>
struct AnimationSystem {
    void Update(World &world, const ModelAssets &modelAssets, float deltaTime);

  private:
    Animator animator_;
};
