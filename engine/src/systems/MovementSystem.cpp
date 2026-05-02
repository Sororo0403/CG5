#include "MovementSystem.h"

#include "Transform.h"
#include "Velocity.h"
#include "World.h"

void MovementSystem::Update(World &world, float deltaTime) {
    world.View<Transform, Velocity>(
        [deltaTime](Entity, Transform &transform, Velocity &velocity) {
            transform.position.x += velocity.linear.x * deltaTime;
            transform.position.y += velocity.linear.y * deltaTime;
            transform.position.z += velocity.linear.z * deltaTime;
        });
}
