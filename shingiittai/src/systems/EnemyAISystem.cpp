#include "EnemyAISystem.h"

#include "Tags.h"
#include "Transform.h"
#include "Velocity.h"
#include "World.h"

#include <cmath>

void EnemyAISystem::Update(World &world, float deltaTime) {
    (void)deltaTime;

    const Transform *playerTransform = nullptr;
    world.View<PlayerTag, Transform>(
        [&playerTransform](Entity, PlayerTag &, Transform &transform) {
            playerTransform = &transform;
        });

    if (playerTransform == nullptr) {
        return;
    }

    world.View<EnemyTag, Transform, Velocity>(
        [playerTransform](Entity, EnemyTag &, Transform &enemyTransform,
                          Velocity &velocity) {
            constexpr float kChaseSpeed = 3.0f;

            const float dx =
                playerTransform->position.x - enemyTransform.position.x;
            const float dz =
                playerTransform->position.z - enemyTransform.position.z;
            const float length = std::sqrt(dx * dx + dz * dz);

            if (length <= 0.001f) {
                velocity.linear.x = 0.0f;
                velocity.linear.z = 0.0f;
                return;
            }

            velocity.linear.x = dx / length * kChaseSpeed;
            velocity.linear.z = dz / length * kChaseSpeed;
        });
}
