#include "CollisionSystem.h"

#include "World.h"

#include <cmath>

void CollisionSystem::Update(World &world) {
    events_.clear();

    world.View<Transform, Collider>(
        [this, &world](Entity a, Transform &ta, Collider &ca) {
            world.View<Transform, Collider>(
                [this, a, &ta, &ca](Entity b, Transform &tb, Collider &cb) {
                    if (a >= b || !CanCollide(ca, cb)) {
                        return;
                    }

                    if (Intersects(ta, ca, tb, cb)) {
                        events_.push_back({a, b});
                    }
                });
        });
}

const std::vector<CollisionEvent> &CollisionSystem::Events() const {
    return events_;
}

bool CollisionSystem::CanCollide(const Collider &a, const Collider &b) const {
    return (a.mask & b.layer) != 0 && (b.mask & a.layer) != 0;
}

bool CollisionSystem::Intersects(const Transform &aTransform,
                                 const Collider &a,
                                 const Transform &bTransform,
                                 const Collider &b) const {
    if (a.shape == ColliderShape::Sphere || b.shape == ColliderShape::Sphere) {
        return IntersectsSphereFallback(aTransform, a, bTransform, b);
    }

    return IntersectsAabb(aTransform, a, bTransform, b);
}

bool CollisionSystem::IntersectsAabb(const Transform &aTransform,
                                     const Collider &a,
                                     const Transform &bTransform,
                                     const Collider &b) const {
    const DirectX::XMFLOAT3 ac = WorldCenter(aTransform, a);
    const DirectX::XMFLOAT3 bc = WorldCenter(bTransform, b);
    const DirectX::XMFLOAT3 ah = WorldHalfSize(aTransform, a);
    const DirectX::XMFLOAT3 bh = WorldHalfSize(bTransform, b);

    return std::abs(ac.x - bc.x) <= (ah.x + bh.x) &&
           std::abs(ac.y - bc.y) <= (ah.y + bh.y) &&
           std::abs(ac.z - bc.z) <= (ah.z + bh.z);
}

bool CollisionSystem::IntersectsSphereFallback(const Transform &aTransform,
                                               const Collider &a,
                                               const Transform &bTransform,
                                               const Collider &b) const {
    const DirectX::XMFLOAT3 ac = WorldCenter(aTransform, a);
    const DirectX::XMFLOAT3 bc = WorldCenter(bTransform, b);
    const float ar = WorldRadius(aTransform, a);
    const float br = WorldRadius(bTransform, b);
    const float dx = ac.x - bc.x;
    const float dy = ac.y - bc.y;
    const float dz = ac.z - bc.z;
    const float distanceSq = dx * dx + dy * dy + dz * dz;
    const float radius = ar + br;

    return distanceSq <= radius * radius;
}

DirectX::XMFLOAT3 CollisionSystem::WorldCenter(
    const Transform &transform, const Collider &collider) const {
    return {transform.position.x + collider.center.x * transform.scale.x,
            transform.position.y + collider.center.y * transform.scale.y,
            transform.position.z + collider.center.z * transform.scale.z};
}

float CollisionSystem::WorldRadius(const Transform &transform,
                                   const Collider &collider) const {
    float scale = std::abs(transform.scale.x);
    if (scale < std::abs(transform.scale.y)) {
        scale = std::abs(transform.scale.y);
    }
    if (scale < std::abs(transform.scale.z)) {
        scale = std::abs(transform.scale.z);
    }
    if (collider.shape == ColliderShape::Sphere) {
        return collider.radius * scale;
    }

    const DirectX::XMFLOAT3 halfSize = WorldHalfSize(transform, collider);
    return std::sqrt(halfSize.x * halfSize.x + halfSize.y * halfSize.y +
                     halfSize.z * halfSize.z);
}

DirectX::XMFLOAT3 CollisionSystem::WorldHalfSize(
    const Transform &transform, const Collider &collider) const {
    return {std::abs(collider.halfSize.x * transform.scale.x),
            std::abs(collider.halfSize.y * transform.scale.y),
            std::abs(collider.halfSize.z * transform.scale.z)};
}
