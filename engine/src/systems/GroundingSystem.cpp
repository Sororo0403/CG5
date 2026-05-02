#include "GroundingSystem.h"

#include "CharacterController.h"
#include "Collider.h"
#include "CollisionBody.h"
#include "GroundingComponents.h"
#include "Transform.h"
#include "Velocity.h"
#include "World.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

struct GroundCandidate {
    Entity entity = kInvalidEntity;
    Transform *transform = nullptr;
    Collider *collider = nullptr;
    GroundSurface *surface = nullptr;
};

float ColliderBottom(const Transform &transform, const Collider &collider) {
    const float centerY = transform.position.y + collider.center.y;
    if (collider.shape == ColliderShape::Capsule) {
        return centerY - collider.capsuleHalfHeight - collider.radius;
    }
    if (collider.shape == ColliderShape::Sphere) {
        return centerY - collider.radius;
    }
    return centerY - std::abs(collider.halfSize.y * transform.scale.y);
}

float ColliderTop(const Transform &transform, const Collider &collider) {
    const float centerY = transform.position.y + collider.center.y;
    if (collider.shape == ColliderShape::Capsule) {
        return centerY + collider.capsuleHalfHeight + collider.radius;
    }
    if (collider.shape == ColliderShape::Sphere) {
        return centerY + collider.radius;
    }
    return centerY + std::abs(collider.halfSize.y * transform.scale.y);
}

bool OverlapsXZ(const Transform &aTransform, const Collider &aCollider,
                const Transform &bTransform, const Collider &bCollider) {
    const float aCenterX = aTransform.position.x + aCollider.center.x;
    const float aCenterZ = aTransform.position.z + aCollider.center.z;
    const float bCenterX = bTransform.position.x + bCollider.center.x;
    const float bCenterZ = bTransform.position.z + bCollider.center.z;

    const float aHalfX = aCollider.shape == ColliderShape::Sphere ||
                                 aCollider.shape == ColliderShape::Capsule
                             ? aCollider.radius
                             : std::abs(aCollider.halfSize.x *
                                        aTransform.scale.x);
    const float aHalfZ = aCollider.shape == ColliderShape::Sphere ||
                                 aCollider.shape == ColliderShape::Capsule
                             ? aCollider.radius
                             : std::abs(aCollider.halfSize.z *
                                        aTransform.scale.z);
    const float bHalfX = bCollider.shape == ColliderShape::Sphere ||
                                 bCollider.shape == ColliderShape::Capsule
                             ? bCollider.radius
                             : std::abs(bCollider.halfSize.x *
                                        bTransform.scale.x);
    const float bHalfZ = bCollider.shape == ColliderShape::Sphere ||
                                 bCollider.shape == ColliderShape::Capsule
                             ? bCollider.radius
                             : std::abs(bCollider.halfSize.z *
                                        bTransform.scale.z);

    return std::abs(aCenterX - bCenterX) <= aHalfX + bHalfX &&
           std::abs(aCenterZ - bCenterZ) <= aHalfZ + bHalfZ;
}

} // namespace

void GroundingSystem::Update(World &world) {
    std::vector<GroundCandidate> grounds;
    world.View<Transform, Collider, CollisionBody>(
        [&grounds, &world](Entity entity, Transform &transform,
                           Collider &collider, CollisionBody &body) {
            if (collider.isTrigger || body.type != CollisionBodyType::Static) {
                return;
            }
            grounds.push_back(
                {entity, &transform, &collider,
                 world.TryGet<GroundSurface>(entity)});
        });

    world.View<Transform, Collider, CollisionBody, GroundedState,
               GroundingAssist>(
        [&grounds, &world](Entity entity, Transform &transform,
                           Collider &collider, CollisionBody &body,
                           GroundedState &grounded,
                           GroundingAssist &assist) {
            if (body.type == CollisionBodyType::Static || collider.isTrigger) {
                return;
            }

            const float bottom = ColliderBottom(transform, collider);
            Entity bestGround = kInvalidEntity;
            float bestTop = -3.402823e+38f;
            DirectX::XMFLOAT3 bestNormal{0.0f, 1.0f, 0.0f};

            for (const GroundCandidate &ground : grounds) {
                if (ground.entity == entity) {
                    continue;
                }
                if (!OverlapsXZ(transform, collider, *ground.transform,
                                *ground.collider)) {
                    continue;
                }

                const float top = ColliderTop(*ground.transform,
                                              *ground.collider);
                const float gap = bottom - top;
                if (gap < -assist.stepHeight || gap > assist.snapDistance) {
                    continue;
                }
                if (top > bestTop) {
                    bestTop = top;
                    bestGround = ground.entity;
                    bestNormal = ground.surface ? ground.surface->normal
                                                : DirectX::XMFLOAT3{0.0f, 1.0f,
                                                                    0.0f};
                }
            }

            if (!bestGround.IsValid()) {
                return;
            }

            const float correction = bestTop - bottom + assist.skinWidth;
            transform.position.y += correction;
            grounded.grounded = true;
            grounded.groundEntity = bestGround;
            grounded.normal = bestNormal;
            grounded.timeSinceGrounded = 0.0f;

            if (Velocity *velocity = world.TryGet<Velocity>(entity)) {
                if (velocity->linear.y < 0.0f) {
                    velocity->linear.y = 0.0f;
                }
            }
        });
}
