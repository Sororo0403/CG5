#include "LockOnSystem.h"

#include "ActionInput.h"
#include "LockOn.h"
#include "Transform.h"
#include "World.h"

#include <cmath>

namespace {

float DistanceSq(const Transform &a, const Transform &b) {
    const float dx = a.position.x - b.position.x;
    const float dy = a.position.y - b.position.y;
    const float dz = a.position.z - b.position.z;
    return dx * dx + dy * dy + dz * dz;
}

} // namespace

void LockOnSystem::Update(World &world) {
    world.View<LockOnState, ActionInput, Transform>(
        [&world](Entity, LockOnState &state, ActionInput &input,
                 Transform &transform) {
            if (state.locked) {
                const Transform *targetTransform =
                    world.TryGet<Transform>(state.target);
                const LockOnTarget *target =
                    world.TryGet<LockOnTarget>(state.target);
                const float releaseDistanceSq =
                    state.releaseDistance * state.releaseDistance;
                if (!world.IsAlive(state.target) || !targetTransform ||
                    !target || !target->enabled ||
                    DistanceSq(transform, *targetTransform) >
                        releaseDistanceSq) {
                    state.target = kInvalidEntity;
                    state.locked = false;
                }
            }

            if (!input.lockOnPressed) {
                return;
            }

            if (state.locked) {
                state.target = kInvalidEntity;
                state.locked = false;
                return;
            }

            Entity bestTarget = kInvalidEntity;
            float bestScore = 0.0f;
            const float maxDistanceSq = state.maxDistance * state.maxDistance;
            world.View<LockOnTarget, Transform>(
                [&](Entity candidate, LockOnTarget &target,
                    Transform &candidateTransform) {
                    if (!target.enabled) {
                        return;
                    }

                    const float distanceSq =
                        DistanceSq(transform, candidateTransform);
                    if (distanceSq > maxDistanceSq) {
                        return;
                    }

                    const float score =
                        target.priority / (distanceSq + 0.001f);
                    if (!bestTarget.IsValid() || score > bestScore) {
                        bestTarget = candidate;
                        bestScore = score;
                    }
                });

            state.target = bestTarget;
            state.locked = bestTarget.IsValid();
        });
}
