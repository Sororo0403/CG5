#include "ContinuousCollisionSystem.h"

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

struct StaticSolid {
    Entity entity = kInvalidEntity;
    Transform *transform = nullptr;
    Collider *collider = nullptr;
    GroundSurface *surface = nullptr;
};

float BottomAt(const Transform &transform, const Collider &collider) {
    const float centerY = transform.position.y + collider.center.y;
    if (collider.shape == ColliderShape::Capsule) {
        return centerY - collider.capsuleHalfHeight - collider.radius;
    }
    if (collider.shape == ColliderShape::Sphere) {
        return centerY - collider.radius;
    }
    return centerY - std::abs(collider.halfSize.y * transform.scale.y);
}

float TopAt(const Transform &transform, const Collider &collider) {
    const float centerY = transform.position.y + collider.center.y;
    if (collider.shape == ColliderShape::Capsule) {
        return centerY + collider.capsuleHalfHeight + collider.radius;
    }
    if (collider.shape == ColliderShape::Sphere) {
        return centerY + collider.radius;
    }
    return centerY + std::abs(collider.halfSize.y * transform.scale.y);
}

bool SweptXzOverlaps(const Transform &previous, const Transform &current,
                     const Collider &moving, const Transform &solidTransform,
                     const Collider &solid) {
    const float minX =
        (std::min)(previous.position.x + moving.center.x,
                   current.position.x + moving.center.x);
    const float maxX =
        (std::max)(previous.position.x + moving.center.x,
                   current.position.x + moving.center.x);
    const float minZ =
        (std::min)(previous.position.z + moving.center.z,
                   current.position.z + moving.center.z);
    const float maxZ =
        (std::max)(previous.position.z + moving.center.z,
                   current.position.z + moving.center.z);

    const float movingHalfX = moving.shape == ColliderShape::Sphere ||
                                      moving.shape == ColliderShape::Capsule
                                  ? moving.radius
                                  : std::abs(moving.halfSize.x *
                                             current.scale.x);
    const float movingHalfZ = moving.shape == ColliderShape::Sphere ||
                                      moving.shape == ColliderShape::Capsule
                                  ? moving.radius
                                  : std::abs(moving.halfSize.z *
                                             current.scale.z);
    const float solidHalfX = solid.shape == ColliderShape::Sphere ||
                                     solid.shape == ColliderShape::Capsule
                                 ? solid.radius
                                 : std::abs(solid.halfSize.x *
                                            solidTransform.scale.x);
    const float solidHalfZ = solid.shape == ColliderShape::Sphere ||
                                     solid.shape == ColliderShape::Capsule
                                 ? solid.radius
                                 : std::abs(solid.halfSize.z *
                                            solidTransform.scale.z);

    const float solidX = solidTransform.position.x + solid.center.x;
    const float solidZ = solidTransform.position.z + solid.center.z;
    return minX - movingHalfX <= solidX + solidHalfX &&
           maxX + movingHalfX >= solidX - solidHalfX &&
           minZ - movingHalfZ <= solidZ + solidHalfZ &&
           maxZ + movingHalfZ >= solidZ - solidHalfZ;
}

} // namespace

void ContinuousCollisionSystem::Update(World &world) {
    std::vector<StaticSolid> solids;
    world.View<Transform, Collider, CollisionBody>(
        [&world, &solids](Entity entity, Transform &transform,
                          Collider &collider, CollisionBody &body) {
            if (collider.isTrigger || body.type != CollisionBodyType::Static) {
                return;
            }
            solids.push_back(
                {entity, &transform, &collider,
                 world.TryGet<GroundSurface>(entity)});
        });

    world.View<Transform, PreviousTransform, Collider, CollisionBody>(
        [&world, &solids](Entity entity, Transform &transform,
                          PreviousTransform &previous, Collider &collider,
                          CollisionBody &body) {
            if (body.type == CollisionBodyType::Static || collider.isTrigger) {
                return;
            }

            const float prevBottom = BottomAt(previous.transform, collider);
            const float currBottom = BottomAt(transform, collider);
            if (currBottom >= prevBottom) {
                return;
            }

            Entity bestGround = kInvalidEntity;
            float bestTop = -3.402823e+38f;
            DirectX::XMFLOAT3 bestNormal{0.0f, 1.0f, 0.0f};
            for (const StaticSolid &solid : solids) {
                if (!SweptXzOverlaps(previous.transform, transform, collider,
                                     *solid.transform, *solid.collider)) {
                    continue;
                }

                const float top = TopAt(*solid.transform, *solid.collider);
                if (prevBottom >= top && currBottom <= top && top > bestTop) {
                    bestGround = solid.entity;
                    bestTop = top;
                    bestNormal = solid.surface ? solid.surface->normal
                                               : DirectX::XMFLOAT3{0.0f, 1.0f,
                                                                   0.0f};
                }
            }

            if (!bestGround.IsValid()) {
                return;
            }

            transform.position.y += bestTop - currBottom;
            if (Velocity *velocity = world.TryGet<Velocity>(entity)) {
                if (velocity->linear.y < 0.0f) {
                    velocity->linear.y = 0.0f;
                }
            }
            if (GroundedState *grounded = world.TryGet<GroundedState>(entity)) {
                grounded->grounded = true;
                grounded->groundEntity = bestGround;
                grounded->normal = bestNormal;
                grounded->timeSinceGrounded = 0.0f;
            }
        });
}
