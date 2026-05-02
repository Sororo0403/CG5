#include "CollisionSystem.h"

#include "World.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_set>

using namespace DirectX;

namespace {

struct WorldObb {
    XMFLOAT3 center{0.0f, 0.0f, 0.0f};
    XMFLOAT3 halfSize{0.5f, 0.5f, 0.5f};
    std::array<XMFLOAT3, 3> axes{
        XMFLOAT3{1.0f, 0.0f, 0.0f},
        XMFLOAT3{0.0f, 1.0f, 0.0f},
        XMFLOAT3{0.0f, 0.0f, 1.0f},
    };
};

float Dot(const XMFLOAT3 &a, const XMFLOAT3 &b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

XMFLOAT3 Subtract(const XMFLOAT3 &a, const XMFLOAT3 &b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

XMFLOAT3 TransformAxis(const XMFLOAT4 &rotation, const XMVECTOR &axis) {
    const XMVECTOR q = XMQuaternionNormalize(XMLoadFloat4(&rotation));
    XMFLOAT3 result{};
    XMStoreFloat3(&result, XMVector3Normalize(XMVector3Rotate(axis, q)));
    return result;
}

WorldObb MakeWorldObb(const Transform &transform, const Collider &collider) {
    WorldObb box{};

    XMFLOAT3 scaledCenter{
        collider.center.x * transform.scale.x,
        collider.center.y * transform.scale.y,
        collider.center.z * transform.scale.z,
    };

    if (collider.shape == ColliderShape::OBB) {
        const XMVECTOR q = XMQuaternionNormalize(XMLoadFloat4(&transform.rotation));
        XMFLOAT3 rotatedCenter{};
        XMStoreFloat3(&rotatedCenter,
                      XMVector3Rotate(XMLoadFloat3(&scaledCenter), q));
        scaledCenter = rotatedCenter;

        box.axes[0] = TransformAxis(transform.rotation,
                                    XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
        box.axes[1] = TransformAxis(transform.rotation,
                                    XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
        box.axes[2] = TransformAxis(transform.rotation,
                                    XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
    }

    box.center = {
        transform.position.x + scaledCenter.x,
        transform.position.y + scaledCenter.y,
        transform.position.z + scaledCenter.z,
    };
    box.halfSize = {
        std::abs(collider.halfSize.x * transform.scale.x),
        std::abs(collider.halfSize.y * transform.scale.y),
        std::abs(collider.halfSize.z * transform.scale.z),
    };
    return box;
}

float HalfSizeAt(const XMFLOAT3 &halfSize, size_t index) {
    if (index == 0) {
        return halfSize.x;
    }
    if (index == 1) {
        return halfSize.y;
    }
    return halfSize.z;
}

} // namespace

void CollisionSystem::Update(World &world) {
    events_.clear();
    std::unordered_set<CollisionPair, CollisionPairHash> currentPairs;

    world.View<Transform, Collider>(
        [this, &world, &currentPairs](Entity a, Transform &ta, Collider &ca) {
            world.View<Transform, Collider>(
                [this, a, &ta, &ca, &currentPairs](
                    Entity b, Transform &tb, Collider &cb) {
                    if (a >= b || !CanCollide(ca, cb)) {
                        return;
                    }

                    if (Intersects(ta, ca, tb, cb)) {
                        const CollisionPair pair = MakePair(a, b);
                        currentPairs.insert(pair);
                        const CollisionEventType type =
                            previousPairs_.find(pair) == previousPairs_.end()
                                ? CollisionEventType::Enter
                                : CollisionEventType::Stay;
                        events_.push_back({pair.a, pair.b, type});
                    }
                });
        });

    for (const CollisionPair &pair : previousPairs_) {
        if (currentPairs.find(pair) == currentPairs.end()) {
            events_.push_back({pair.a, pair.b, CollisionEventType::Exit});
        }
    }

    previousPairs_ = std::move(currentPairs);
}

const std::vector<CollisionEvent> &CollisionSystem::Events() const {
    return events_;
}

CollisionSystem::CollisionPair CollisionSystem::MakePair(Entity a,
                                                         Entity b) const {
    if (b < a) {
        return {b, a};
    }
    return {a, b};
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

    if (a.shape == ColliderShape::OBB || b.shape == ColliderShape::OBB) {
        return IntersectsObb(aTransform, a, bTransform, b);
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

bool CollisionSystem::IntersectsObb(const Transform &aTransform,
                                    const Collider &a,
                                    const Transform &bTransform,
                                    const Collider &b) const {
    constexpr float kEpsilon = 1.0e-5f;

    const WorldObb boxA = MakeWorldObb(aTransform, a);
    const WorldObb boxB = MakeWorldObb(bTransform, b);
    const XMFLOAT3 centerDelta = Subtract(boxB.center, boxA.center);

    float rotation[3][3]{};
    float absRotation[3][3]{};
    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 3; ++j) {
            rotation[i][j] = Dot(boxA.axes[i], boxB.axes[j]);
            absRotation[i][j] = std::abs(rotation[i][j]) + kEpsilon;
        }
    }

    float translation[3]{
        Dot(centerDelta, boxA.axes[0]),
        Dot(centerDelta, boxA.axes[1]),
        Dot(centerDelta, boxA.axes[2]),
    };

    for (size_t i = 0; i < 3; ++i) {
        const float ra = HalfSizeAt(boxA.halfSize, i);
        const float rb = boxB.halfSize.x * absRotation[i][0] +
                         boxB.halfSize.y * absRotation[i][1] +
                         boxB.halfSize.z * absRotation[i][2];
        if (std::abs(translation[i]) > ra + rb) {
            return false;
        }
    }

    for (size_t j = 0; j < 3; ++j) {
        const float ra = boxA.halfSize.x * absRotation[0][j] +
                         boxA.halfSize.y * absRotation[1][j] +
                         boxA.halfSize.z * absRotation[2][j];
        const float rb = HalfSizeAt(boxB.halfSize, j);
        const float distance =
            std::abs(translation[0] * rotation[0][j] +
                     translation[1] * rotation[1][j] +
                     translation[2] * rotation[2][j]);
        if (distance > ra + rb) {
            return false;
        }
    }

    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 3; ++j) {
            const size_t i1 = (i + 1) % 3;
            const size_t i2 = (i + 2) % 3;
            const size_t j1 = (j + 1) % 3;
            const size_t j2 = (j + 2) % 3;

            const float ra = HalfSizeAt(boxA.halfSize, i1) *
                                 absRotation[i2][j] +
                             HalfSizeAt(boxA.halfSize, i2) *
                                 absRotation[i1][j];
            const float rb = HalfSizeAt(boxB.halfSize, j1) *
                                 absRotation[i][j2] +
                             HalfSizeAt(boxB.halfSize, j2) *
                                 absRotation[i][j1];
            const float distance =
                std::abs(translation[i2] * rotation[i1][j] -
                         translation[i1] * rotation[i2][j]);
            if (distance > ra + rb) {
                return false;
            }
        }
    }

    return true;
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
