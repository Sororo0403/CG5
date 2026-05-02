#include "PhysicsSystem.h"

#include "PhysicsBody.h"
#include "Velocity.h"
#include "World.h"

void PhysicsSystem::Update(World &world, float deltaTime) {
    world.View<PhysicsBody, Velocity>(
        [this, deltaTime](Entity, PhysicsBody &body, Velocity &velocity) {
            velocity.linear.x += body.acceleration.x * deltaTime;
            velocity.linear.y += body.acceleration.y * deltaTime;
            velocity.linear.z += body.acceleration.z * deltaTime;

            if (body.useGravity) {
                velocity.linear.y += gravity_.y * body.gravityScale * deltaTime;
            }
        });
}

void PhysicsSystem::SetGravity(const DirectX::XMFLOAT3 &gravity) {
    gravity_ = gravity;
}
