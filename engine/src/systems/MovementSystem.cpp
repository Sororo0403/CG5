#include "MovementSystem.h"

#include "TransformHierarchy.h"
#include "Transform.h"
#include "Velocity.h"
#include "World.h"

void MovementSystem::Update(World &world, float deltaTime) {
    world.View<Transform, Velocity>(
        [&world, deltaTime](Entity entity, Transform &transform,
                            Velocity &velocity) {
            Transform *target = &transform;
            if (LocalTransform *local = world.TryGet<LocalTransform>(entity)) {
                target = &local->transform;
            }

            target->position.x += velocity.linear.x * deltaTime;
            target->position.y += velocity.linear.y * deltaTime;
            target->position.z += velocity.linear.z * deltaTime;
        });
}
