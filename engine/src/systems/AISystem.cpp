#include "AISystem.h"

#include "AIController.h"
#include "Transform.h"
#include "Velocity.h"
#include "World.h"

#include <algorithm>
#include <cmath>

namespace {

float Approach(float current, float target, float amount) {
    if (current < target) {
        return (std::min)(current + amount, target);
    }
    return (std::max)(current - amount, target);
}

} // namespace

void AISystem::Update(World &world, float deltaTime) {
    world.View<PursuitAI, Transform, Velocity>(
        [&world, deltaTime](Entity, PursuitAI &ai, Transform &transform,
                            Velocity &velocity) {
            const Transform *target = world.TryGet<Transform>(ai.target);
            if (!target) {
                velocity.linear.x = Approach(velocity.linear.x, 0.0f,
                                             ai.acceleration * deltaTime);
                velocity.linear.z = Approach(velocity.linear.z, 0.0f,
                                             ai.acceleration * deltaTime);
                return;
            }

            const float dx = target->position.x - transform.position.x;
            const float dz = target->position.z - transform.position.z;
            const float distanceSq = dx * dx + dz * dz;
            const float stopSq = ai.stopDistance * ai.stopDistance;
            const float aggroSq = ai.aggroDistance * ai.aggroDistance;
            float targetX = 0.0f;
            float targetZ = 0.0f;

            if (distanceSq > stopSq && distanceSq <= aggroSq) {
                const float invLength =
                    1.0f / std::sqrt((std::max)(distanceSq, 0.000001f));
                targetX = dx * invLength * ai.moveSpeed;
                targetZ = dz * invLength * ai.moveSpeed;
            }

            velocity.linear.x = Approach(velocity.linear.x, targetX,
                                         ai.acceleration * deltaTime);
            velocity.linear.z = Approach(velocity.linear.z, targetZ,
                                         ai.acceleration * deltaTime);
        });
}
