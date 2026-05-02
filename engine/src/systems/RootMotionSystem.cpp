#include "RootMotionSystem.h"

#include "AnimationComponents.h"
#include "RootMotion.h"
#include "Transform.h"
#include "World.h"

namespace {

DirectX::XMFLOAT3 ExtractTranslation(const DirectX::XMFLOAT4X4 &matrix) {
    return {matrix._41, matrix._42, matrix._43};
}

} // namespace

void RootMotionSystem::Update(World &world) {
    world.View<Transform, SkeletonPoseComponent, AnimationComponent, RootMotion,
               RootMotionState>(
        [](Entity, Transform &transform, SkeletonPoseComponent &pose,
           AnimationComponent &animation, RootMotion &rootMotion,
           RootMotionState &state) {
            if (!pose.hasRootAnimation || !rootMotion.applyTranslation) {
                state.initialized = false;
                return;
            }

            const DirectX::XMFLOAT3 current =
                ExtractTranslation(pose.rootAnimationMatrix);
            if (!state.initialized ||
                state.animationName != animation.currentAnimation ||
                animation.time < animation.previousTime) {
                state.previousRootPosition = current;
                state.animationName = animation.currentAnimation;
                state.initialized = true;
                return;
            }

            DirectX::XMFLOAT3 delta{
                (current.x - state.previousRootPosition.x) * rootMotion.scale,
                (current.y - state.previousRootPosition.y) * rootMotion.scale,
                (current.z - state.previousRootPosition.z) * rootMotion.scale};
            if (!rootMotion.applyVertical) {
                delta.y = 0.0f;
            }

            transform.position.x += delta.x;
            transform.position.y += delta.y;
            transform.position.z += delta.z;
            state.previousRootPosition = current;
        });
}
