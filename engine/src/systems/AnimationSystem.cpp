#include "AnimationSystem.h"

#include "AnimationComponents.h"
#include "MeshRenderer.h"
#include "ModelAssets.h"
#include "World.h"

void AnimationSystem::Update(World &world, const ModelAssets &modelAssets,
                             float deltaTime) {
    world.View<MeshRenderer, AnimationComponent, SkeletonPoseComponent>(
        [this, &modelAssets, deltaTime](
            Entity, MeshRenderer &meshRenderer, AnimationComponent &animation,
            SkeletonPoseComponent &pose) {
            const Model *model = modelAssets.GetModel(meshRenderer.modelId);
            if (!model) {
                return;
            }

            animator_.Update(*model, animation, pose, deltaTime);
        });
}
