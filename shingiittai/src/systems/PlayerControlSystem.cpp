#include "PlayerControlSystem.h"

#include "InputComponent.h"
#include "Tags.h"
#include "Velocity.h"
#include "World.h"

void PlayerControlSystem::Update(World &world, float deltaTime) {
    (void)deltaTime;

    world.View<PlayerTag, InputComponent, Velocity>(
        [](Entity, PlayerTag &, InputComponent &input, Velocity &velocity) {
            constexpr float kMoveSpeed = 8.0f;
            constexpr float kDashScale = 1.8f;

            const float speed = input.dash ? kMoveSpeed * kDashScale
                                           : kMoveSpeed;

            velocity.linear.x = input.move.x * speed;
            velocity.linear.z = input.move.y * speed;
        });
}
