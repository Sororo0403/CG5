#include "CollisionResolutionSystem.h"

#include "CharacterController.h"
#include "Collider.h"
#include "CollisionBody.h"
#include "Transform.h"
#include "Velocity.h"
#include "World.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

struct Solid {
    Entity entity = kInvalidEntity;
    Transform *transform = nullptr;
    Collider *collider = nullptr;
    CollisionBody *body = nullptr;
};

struct SimpleShape {
    DirectX::XMFLOAT3 center{0.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 halfSize{0.5f, 0.5f, 0.5f};
    float radius = 0.5f;
    bool sphere = false;
    bool capsule = false;
    float capsuleHalfHeight = 0.5f;
};

struct Resolution {
    bool hit = false;
    DirectX::XMFLOAT3 normal{0.0f, 1.0f, 0.0f};
    float depth = 0.0f;
};

SimpleShape MakeShape(const Transform &transform, const Collider &collider) {
    SimpleShape shape{};
    shape.center = {transform.position.x + collider.center.x,
                    transform.position.y + collider.center.y,
                    transform.position.z + collider.center.z};
    shape.sphere = collider.shape == ColliderShape::Sphere;
    shape.capsule = collider.shape == ColliderShape::Capsule;

    if (shape.sphere || shape.capsule) {
        const float scale =
            (std::max)({std::abs(transform.scale.x),
                        std::abs(transform.scale.y),
                        std::abs(transform.scale.z)});
        shape.radius = collider.radius * scale;
        shape.capsuleHalfHeight =
            collider.capsuleHalfHeight * std::abs(transform.scale.y);
        shape.halfSize = {shape.radius,
                          shape.radius +
                              (shape.capsule ? shape.capsuleHalfHeight
                                             : 0.0f),
                          shape.radius};
    } else {
        shape.halfSize = {std::abs(collider.halfSize.x * transform.scale.x),
                          std::abs(collider.halfSize.y * transform.scale.y),
                          std::abs(collider.halfSize.z * transform.scale.z)};
    }

    return shape;
}

DirectX::XMFLOAT3 Subtract(const DirectX::XMFLOAT3 &a,
                           const DirectX::XMFLOAT3 &b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

float LengthSq(const DirectX::XMFLOAT3 &value) {
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

DirectX::XMFLOAT3 NormalizeOrUp(const DirectX::XMFLOAT3 &value) {
    const float lengthSq = LengthSq(value);
    if (lengthSq <= 0.000001f) {
        return {0.0f, 1.0f, 0.0f};
    }
    const float invLength = 1.0f / std::sqrt(lengthSq);
    return {value.x * invLength, value.y * invLength, value.z * invLength};
}

DirectX::XMFLOAT3 Add(const DirectX::XMFLOAT3 &a,
                      const DirectX::XMFLOAT3 &b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

DirectX::XMFLOAT3 Scale(const DirectX::XMFLOAT3 &value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

DirectX::XMFLOAT3 ClosestPointOnSegment(const DirectX::XMFLOAT3 &a,
                                        const DirectX::XMFLOAT3 &b,
                                        const DirectX::XMFLOAT3 &point) {
    const DirectX::XMFLOAT3 ab = Subtract(b, a);
    const float lengthSq = LengthSq(ab);
    if (lengthSq <= 0.000001f) {
        return a;
    }

    const float t = std::clamp(
        (Subtract(point, a).x * ab.x + Subtract(point, a).y * ab.y +
         Subtract(point, a).z * ab.z) /
            lengthSq,
        0.0f, 1.0f);
    return Add(a, Scale(ab, t));
}

void CapsuleSegment(const SimpleShape &capsule, DirectX::XMFLOAT3 &a,
                    DirectX::XMFLOAT3 &b) {
    a = {capsule.center.x, capsule.center.y - capsule.capsuleHalfHeight,
         capsule.center.z};
    b = {capsule.center.x, capsule.center.y + capsule.capsuleHalfHeight,
         capsule.center.z};
}

Resolution ResolveSphereSphereLike(const DirectX::XMFLOAT3 &aCenter,
                                   float aRadius,
                                   const DirectX::XMFLOAT3 &bCenter,
                                   float bRadius) {
    const DirectX::XMFLOAT3 delta = Subtract(aCenter, bCenter);
    const float radius = aRadius + bRadius;
    const float distanceSq = LengthSq(delta);
    if (distanceSq >= radius * radius) {
        return {};
    }

    const float distance = std::sqrt((std::max)(distanceSq, 0.000001f));
    return {true, NormalizeOrUp(delta), radius - distance};
}

Resolution ResolveAabbAabb(const SimpleShape &a, const SimpleShape &b) {
    const float overlapX =
        a.halfSize.x + b.halfSize.x - std::abs(a.center.x - b.center.x);
    const float overlapY =
        a.halfSize.y + b.halfSize.y - std::abs(a.center.y - b.center.y);
    const float overlapZ =
        a.halfSize.z + b.halfSize.z - std::abs(a.center.z - b.center.z);

    if (overlapX <= 0.0f || overlapY <= 0.0f || overlapZ <= 0.0f) {
        return {};
    }

    Resolution result{};
    result.hit = true;
    result.depth = overlapX;
    result.normal = {a.center.x < b.center.x ? -1.0f : 1.0f, 0.0f, 0.0f};

    if (overlapY < result.depth) {
        result.depth = overlapY;
        result.normal = {0.0f, a.center.y < b.center.y ? -1.0f : 1.0f, 0.0f};
    }
    if (overlapZ < result.depth) {
        result.depth = overlapZ;
        result.normal = {0.0f, 0.0f, a.center.z < b.center.z ? -1.0f : 1.0f};
    }

    return result;
}

Resolution ResolveSphereSphere(const SimpleShape &a, const SimpleShape &b) {
    return ResolveSphereSphereLike(a.center, a.radius, b.center, b.radius);
}

Resolution ResolveSphereAabb(const SimpleShape &sphere, const SimpleShape &box) {
    const DirectX::XMFLOAT3 closest{
        std::clamp(sphere.center.x, box.center.x - box.halfSize.x,
                   box.center.x + box.halfSize.x),
        std::clamp(sphere.center.y, box.center.y - box.halfSize.y,
                   box.center.y + box.halfSize.y),
        std::clamp(sphere.center.z, box.center.z - box.halfSize.z,
                   box.center.z + box.halfSize.z),
    };
    const DirectX::XMFLOAT3 delta = Subtract(sphere.center, closest);
    const float distanceSq = LengthSq(delta);
    if (distanceSq >= sphere.radius * sphere.radius) {
        return {};
    }

    const float distance = std::sqrt((std::max)(distanceSq, 0.000001f));
    return {true, NormalizeOrUp(delta), sphere.radius - distance};
}

Resolution ResolveCapsuleCapsule(const SimpleShape &a, const SimpleShape &b) {
    DirectX::XMFLOAT3 a0{};
    DirectX::XMFLOAT3 a1{};
    DirectX::XMFLOAT3 b0{};
    DirectX::XMFLOAT3 b1{};
    CapsuleSegment(a, a0, a1);
    CapsuleSegment(b, b0, b1);
    const DirectX::XMFLOAT3 aClosest = ClosestPointOnSegment(a0, a1, b.center);
    const DirectX::XMFLOAT3 bClosest = ClosestPointOnSegment(b0, b1, aClosest);
    return ResolveSphereSphereLike(aClosest, a.radius, bClosest, b.radius);
}

Resolution ResolveSphereCapsule(const SimpleShape &sphere,
                                const SimpleShape &capsule) {
    DirectX::XMFLOAT3 a{};
    DirectX::XMFLOAT3 b{};
    CapsuleSegment(capsule, a, b);
    const DirectX::XMFLOAT3 closest =
        ClosestPointOnSegment(a, b, sphere.center);
    return ResolveSphereSphereLike(sphere.center, sphere.radius, closest,
                                   capsule.radius);
}

Resolution ResolveCapsuleAabb(const SimpleShape &capsule,
                              const SimpleShape &box) {
    DirectX::XMFLOAT3 a{};
    DirectX::XMFLOAT3 b{};
    CapsuleSegment(capsule, a, b);
    const DirectX::XMFLOAT3 closestOnSegment =
        ClosestPointOnSegment(a, b, box.center);
    SimpleShape sphere = capsule;
    sphere.sphere = true;
    sphere.capsule = false;
    sphere.center = closestOnSegment;

    Resolution result = ResolveSphereAabb(sphere, box);
    if (result.hit) {
        return result;
    }

    sphere.center = a;
    result = ResolveSphereAabb(sphere, box);
    if (result.hit) {
        return result;
    }

    sphere.center = b;
    return ResolveSphereAabb(sphere, box);
}

Resolution ResolveShapes(const SimpleShape &a, const SimpleShape &b) {
    if (a.capsule && b.capsule) {
        return ResolveCapsuleCapsule(a, b);
    }
    if (a.sphere && b.capsule) {
        return ResolveSphereCapsule(a, b);
    }
    if (a.capsule && b.sphere) {
        Resolution result = ResolveSphereCapsule(b, a);
        result.normal.x = -result.normal.x;
        result.normal.y = -result.normal.y;
        result.normal.z = -result.normal.z;
        return result;
    }
    if (a.capsule) {
        return ResolveCapsuleAabb(a, b);
    }
    if (b.capsule) {
        Resolution result = ResolveCapsuleAabb(b, a);
        result.normal.x = -result.normal.x;
        result.normal.y = -result.normal.y;
        result.normal.z = -result.normal.z;
        return result;
    }

    if (a.sphere && b.sphere) {
        return ResolveSphereSphere(a, b);
    }
    if (a.sphere) {
        return ResolveSphereAabb(a, b);
    }
    if (b.sphere) {
        Resolution result = ResolveSphereAabb(b, a);
        result.normal.x = -result.normal.x;
        result.normal.y = -result.normal.y;
        result.normal.z = -result.normal.z;
        return result;
    }
    return ResolveAabbAabb(a, b);
}

bool IsMovable(const CollisionBody &body) {
    return body.type == CollisionBodyType::Dynamic ||
           body.type == CollisionBodyType::Kinematic;
}

void Move(Solid &solid, const DirectX::XMFLOAT3 &amount) {
    solid.transform->position.x += amount.x;
    solid.transform->position.y += amount.y;
    solid.transform->position.z += amount.z;
}

void RemoveVelocityInto(World &world, Entity entity,
                        const DirectX::XMFLOAT3 &normal) {
    Velocity *velocity = world.TryGet<Velocity>(entity);
    if (!velocity) {
        return;
    }

    const float into = velocity->linear.x * normal.x +
                       velocity->linear.y * normal.y +
                       velocity->linear.z * normal.z;
    if (into < 0.0f) {
        velocity->linear.x -= normal.x * into;
        velocity->linear.y -= normal.y * into;
        velocity->linear.z -= normal.z * into;
    }
}

void MarkGrounded(World &world, Entity entity, Entity ground,
                  const DirectX::XMFLOAT3 &normal,
                  const CollisionBody &body) {
    if (!body.groundable || normal.y < body.groundNormalThreshold) {
        return;
    }

    GroundedState *grounded = world.TryGet<GroundedState>(entity);
    if (!grounded) {
        return;
    }

    grounded->grounded = true;
    grounded->groundEntity = ground;
    grounded->normal = normal;
    grounded->timeSinceGrounded = 0.0f;
}

} // namespace

void CollisionResolutionSystem::Update(World &world) {
    std::vector<Solid> solids;
    world.View<Transform, Collider, CollisionBody>(
        [&solids](Entity entity, Transform &transform, Collider &collider,
                  CollisionBody &body) {
            if (!body.resolve || collider.isTrigger) {
                return;
            }
            solids.push_back({entity, &transform, &collider, &body});
        });

    for (Solid &solid : solids) {
        GroundedState *grounded = world.TryGet<GroundedState>(solid.entity);
        if (grounded) {
            grounded->grounded = false;
            grounded->groundEntity = kInvalidEntity;
        }
    }

    for (size_t i = 0; i < solids.size(); ++i) {
        for (size_t j = i + 1; j < solids.size(); ++j) {
            Solid &a = solids[i];
            Solid &b = solids[j];
            if (!IsMovable(*a.body) && !IsMovable(*b.body)) {
                continue;
            }

            const SimpleShape aShape = MakeShape(*a.transform, *a.collider);
            const SimpleShape bShape = MakeShape(*b.transform, *b.collider);
            const Resolution resolution = ResolveShapes(aShape, bShape);
            if (!resolution.hit) {
                continue;
            }

            const bool moveA = IsMovable(*a.body);
            const bool moveB = IsMovable(*b.body);
            const float aShare = moveA && moveB ? 0.5f : (moveA ? 1.0f : 0.0f);
            const float bShare = moveA && moveB ? 0.5f : (moveB ? 1.0f : 0.0f);

            const DirectX::XMFLOAT3 aMove{
                resolution.normal.x * resolution.depth * aShare,
                resolution.normal.y * resolution.depth * aShare,
                resolution.normal.z * resolution.depth * aShare};
            const DirectX::XMFLOAT3 bMove{
                -resolution.normal.x * resolution.depth * bShare,
                -resolution.normal.y * resolution.depth * bShare,
                -resolution.normal.z * resolution.depth * bShare};

            if (moveA) {
                Move(a, aMove);
                RemoveVelocityInto(world, a.entity, resolution.normal);
                MarkGrounded(world, a.entity, b.entity, resolution.normal,
                             *a.body);
            }
            if (moveB) {
                Move(b, bMove);
                const DirectX::XMFLOAT3 bNormal{-resolution.normal.x,
                                                -resolution.normal.y,
                                                -resolution.normal.z};
                RemoveVelocityInto(world, b.entity, bNormal);
                MarkGrounded(world, b.entity, a.entity, bNormal, *b.body);
            }
        }
    }
}
