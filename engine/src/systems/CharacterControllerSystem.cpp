#include "CharacterControllerSystem.h"

#include "ActionInput.h"
#include "CharacterController.h"
#include "Transform.h"
#include "Velocity.h"
#include "World.h"
#include "WorldCommandBuffer.h"

#include <algorithm>
#include <cmath>

namespace {

float Approach(float current, float target, float amount) {
    if (current < target) {
        return (std::min)(current + amount, target);
    }
    return (std::max)(current - amount, target);
}

DirectX::XMFLOAT3 HorizontalDirection(const ActionInput &input,
                                      const Transform &transform) {
    DirectX::XMFLOAT3 direction{input.move.x, 0.0f, input.move.y};
    const float length =
        std::sqrt(direction.x * direction.x + direction.z * direction.z);
    if (length > 0.0001f) {
        direction.x /= length;
        direction.z /= length;
        return direction;
    }

    return {0.0f, 0.0f, transform.rotation.y >= 0.0f ? 1.0f : -1.0f};
}

void FaceDirection(Transform &transform, const DirectX::XMFLOAT3 &direction) {
    const float length =
        std::sqrt(direction.x * direction.x + direction.z * direction.z);
    if (length <= 0.0001f) {
        return;
    }

    const float yaw = std::atan2(direction.x, direction.z);
    DirectX::XMVECTOR rotation =
        DirectX::XMQuaternionRotationRollPitchYaw(0.0f, yaw, 0.0f);
    DirectX::XMStoreFloat4(&transform.rotation, rotation);
}

} // namespace

void CharacterControllerSystem::Update(World &world, float deltaTime) {
    WorldCommandBuffer commands;

    world.View<CharacterController, ActionInput, InputBuffer, GroundedState,
               Velocity, Transform>(
        [&world, &commands, deltaTime](Entity entity,
                                       CharacterController &controller,
                                       ActionInput &input, InputBuffer &buffer,
                                       GroundedState &grounded,
                                       Velocity &velocity,
                                       Transform &transform) {
            controller.dashCooldownRemaining =
                (std::max)(0.0f,
                           controller.dashCooldownRemaining - deltaTime);
            controller.dashTimeRemaining =
                (std::max)(0.0f, controller.dashTimeRemaining - deltaTime);

            if (grounded.grounded) {
                grounded.timeSinceGrounded = 0.0f;
            } else {
                grounded.timeSinceGrounded += deltaTime;
            }

            if (Knockback *knockback = world.TryGet<Knockback>(entity)) {
                if (knockback->overrideInput) {
                    velocity.linear = knockback->velocity;
                } else {
                    velocity.linear.x += knockback->velocity.x;
                    velocity.linear.y += knockback->velocity.y;
                    velocity.linear.z += knockback->velocity.z;
                }

                knockback->timeRemaining -= deltaTime;
                if (knockback->timeRemaining <= 0.0f) {
                    commands.Remove<Knockback>(entity);
                }
                return;
            }

            const bool wantsDash =
                input.dashPressed || buffer.dashTime > 0.0f;
            DirectX::XMFLOAT3 moveDirection =
                HorizontalDirection(input, transform);
            if (wantsDash && controller.dashCooldownRemaining <= 0.0f) {
                controller.dashTimeRemaining = controller.dashDuration;
                controller.dashCooldownRemaining = controller.dashCooldown;
                buffer.dashTime = 0.0f;
            }

            if (controller.dashTimeRemaining > 0.0f) {
                velocity.linear.x = moveDirection.x * controller.dashSpeed;
                velocity.linear.z = moveDirection.z * controller.dashSpeed;
            } else {
                const float targetX = input.move.x * controller.moveSpeed;
                const float targetZ = input.move.y * controller.moveSpeed;
                const float accel =
                    grounded.grounded ? controller.groundAcceleration
                                      : controller.airAcceleration;
                velocity.linear.x =
                    Approach(velocity.linear.x, targetX, accel * deltaTime);
                velocity.linear.z =
                    Approach(velocity.linear.z, targetZ, accel * deltaTime);
            }

            const bool canJump =
                grounded.grounded ||
                grounded.timeSinceGrounded <= controller.coyoteTime;
            if ((input.jumpPressed || buffer.jumpTime > 0.0f) && canJump) {
                velocity.linear.y = controller.jumpSpeed;
                grounded.grounded = false;
                grounded.timeSinceGrounded = controller.coyoteTime + 1.0f;
                buffer.jumpTime = 0.0f;
            }

            if (controller.faceMoveDirection &&
                (std::abs(velocity.linear.x) > 0.01f ||
                 std::abs(velocity.linear.z) > 0.01f)) {
                FaceDirection(transform,
                              {velocity.linear.x, 0.0f, velocity.linear.z});
            }

        });

    commands.Playback(world);
}
