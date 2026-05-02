#include "CollisionSystem.h"

#include "TransformHierarchy.h"
#include "World.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace DirectX;

namespace {

struct WorldBox {
    XMFLOAT3 center{0.0f, 0.0f, 0.0f};
    std::array<XMFLOAT3, 3> halfAxes{
        XMFLOAT3{0.5f, 0.0f, 0.0f},
        XMFLOAT3{0.0f, 0.5f, 0.0f},
        XMFLOAT3{0.0f, 0.0f, 0.5f},
    };
};

struct WorldShape {
    Collider collider{};
    WorldBox box{};
    float sphereRadius = 0.5f;
};

struct BroadPhaseBounds {
    XMFLOAT3 min{};
    XMFLOAT3 max{};
};

struct CollisionCandidate {
    Entity entity = kInvalidEntity;
    Collider collider{};
    WorldShape shape{};
    BroadPhaseBounds bounds{};
};

struct GridCell {
    int x = 0;
    int y = 0;
    int z = 0;

    bool operator==(const GridCell &other) const {
        return x == other.x && y == other.y && z == other.z;
    }
};

struct GridCellHash {
    size_t operator()(const GridCell &cell) const {
        const size_t x = static_cast<size_t>(cell.x * 73856093);
        const size_t y = static_cast<size_t>(cell.y * 19349663);
        const size_t z = static_cast<size_t>(cell.z * 83492791);
        return x ^ y ^ z;
    }
};

float Dot(const XMFLOAT3 &a, const XMFLOAT3 &b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

XMFLOAT3 Subtract(const XMFLOAT3 &a, const XMFLOAT3 &b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

XMFLOAT3 Add(const XMFLOAT3 &a, const XMFLOAT3 &b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

XMFLOAT3 Scale(const XMFLOAT3 &v, float scale) {
    return {v.x * scale, v.y * scale, v.z * scale};
}

XMFLOAT3 Cross(const XMFLOAT3 &a, const XMFLOAT3 &b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

float LengthSq(const XMFLOAT3 &v) { return Dot(v, v); }

XMMATRIX ComposeMatrix(const Transform &transform) {
    const XMVECTOR rotation =
        XMQuaternionNormalize(XMLoadFloat4(&transform.rotation));
    return XMMatrixScaling(transform.scale.x, transform.scale.y,
                           transform.scale.z) *
           XMMatrixRotationQuaternion(rotation) *
           XMMatrixTranslation(transform.position.x, transform.position.y,
                               transform.position.z);
}

XMFLOAT3 TransformPoint(const XMMATRIX &matrix, const XMFLOAT3 &point) {
    XMFLOAT3 result{};
    XMStoreFloat3(&result, XMVector3Transform(XMLoadFloat3(&point), matrix));
    return result;
}

XMFLOAT3 TransformVector(const XMMATRIX &matrix, const XMFLOAT3 &vector) {
    XMFLOAT3 result{};
    XMStoreFloat3(&result,
                  XMVector3TransformNormal(XMLoadFloat3(&vector), matrix));
    return result;
}

WorldShape MakeWorldShape(const XMMATRIX &matrix, const Collider &collider) {
    WorldShape shape{};
    shape.collider = collider;
    shape.box.center = TransformPoint(matrix, collider.center);

    const XMFLOAT3 halfAxes[3]{
        {collider.halfSize.x, 0.0f, 0.0f},
        {0.0f, collider.halfSize.y, 0.0f},
        {0.0f, 0.0f, collider.halfSize.z},
    };

    if (collider.shape == ColliderShape::AABB) {
        const XMFLOAT3 worldX = TransformVector(matrix, {1.0f, 0.0f, 0.0f});
        const XMFLOAT3 worldY = TransformVector(matrix, {0.0f, 1.0f, 0.0f});
        const XMFLOAT3 worldZ = TransformVector(matrix, {0.0f, 0.0f, 1.0f});
        shape.box.halfAxes[0] = {
            std::sqrt(LengthSq(worldX)) * collider.halfSize.x, 0.0f, 0.0f};
        shape.box.halfAxes[1] = {
            0.0f, std::sqrt(LengthSq(worldY)) * collider.halfSize.y, 0.0f};
        shape.box.halfAxes[2] = {
            0.0f, 0.0f, std::sqrt(LengthSq(worldZ)) * collider.halfSize.z};
    } else {
        shape.box.halfAxes[0] = TransformVector(matrix, halfAxes[0]);
        shape.box.halfAxes[1] = TransformVector(matrix, halfAxes[1]);
        shape.box.halfAxes[2] = TransformVector(matrix, halfAxes[2]);
    }

    const XMFLOAT3 worldX = TransformVector(matrix, {1.0f, 0.0f, 0.0f});
    const XMFLOAT3 worldY = TransformVector(matrix, {0.0f, 1.0f, 0.0f});
    const XMFLOAT3 worldZ = TransformVector(matrix, {0.0f, 0.0f, 1.0f});
    const float scale = (std::max)({std::sqrt(LengthSq(worldX)),
                                    std::sqrt(LengthSq(worldY)),
                                    std::sqrt(LengthSq(worldZ))});
    shape.sphereRadius = collider.shape == ColliderShape::Sphere
                             ? collider.radius * scale
                             : std::sqrt(LengthSq(shape.box.halfAxes[0]) +
                                         LengthSq(shape.box.halfAxes[1]) +
                                         LengthSq(shape.box.halfAxes[2]));

    return shape;
}

float ProjectionRadius(const WorldBox &box, const XMFLOAT3 &axis) {
    return std::abs(Dot(box.halfAxes[0], axis)) +
           std::abs(Dot(box.halfAxes[1], axis)) +
           std::abs(Dot(box.halfAxes[2], axis));
}

bool SeparatedOnAxis(const WorldBox &a, const WorldBox &b,
                     const XMFLOAT3 &axis) {
    constexpr float kAxisEpsilon = 1.0e-6f;
    if (LengthSq(axis) < kAxisEpsilon) {
        return false;
    }

    const XMFLOAT3 centerDelta = Subtract(b.center, a.center);
    const float distance = std::abs(Dot(centerDelta, axis));
    const float radius = ProjectionRadius(a, axis) + ProjectionRadius(b, axis);
    return distance > radius;
}

bool BoxesIntersect(const WorldBox &a, const WorldBox &b) {
    const std::array<XMFLOAT3, 3> aFaceNormals{
        Cross(a.halfAxes[1], a.halfAxes[2]),
        Cross(a.halfAxes[2], a.halfAxes[0]),
        Cross(a.halfAxes[0], a.halfAxes[1]),
    };
    const std::array<XMFLOAT3, 3> bFaceNormals{
        Cross(b.halfAxes[1], b.halfAxes[2]),
        Cross(b.halfAxes[2], b.halfAxes[0]),
        Cross(b.halfAxes[0], b.halfAxes[1]),
    };

    for (const XMFLOAT3 &axis : aFaceNormals) {
        if (SeparatedOnAxis(a, b, axis)) {
            return false;
        }
    }
    for (const XMFLOAT3 &axis : bFaceNormals) {
        if (SeparatedOnAxis(a, b, axis)) {
            return false;
        }
    }
    for (const XMFLOAT3 &aAxis : a.halfAxes) {
        for (const XMFLOAT3 &bAxis : b.halfAxes) {
            if (SeparatedOnAxis(a, b, Cross(aAxis, bAxis))) {
                return false;
            }
        }
    }
    return true;
}

XMFLOAT3 ClosestPointOnBox(const WorldBox &box, const XMFLOAT3 &point) {
    XMFLOAT3 closest = box.center;
    const XMFLOAT3 centerToPoint = Subtract(point, box.center);

    for (const XMFLOAT3 &halfAxis : box.halfAxes) {
        const float axisLengthSq = LengthSq(halfAxis);
        if (axisLengthSq <= 1.0e-6f) {
            continue;
        }

        const float amount =
            std::clamp(Dot(centerToPoint, halfAxis) / axisLengthSq, -1.0f,
                       1.0f);
        closest = Add(closest, Scale(halfAxis, amount));
    }

    return closest;
}

bool SphereIntersectsBox(const WorldShape &sphere, const WorldShape &box) {
    const XMFLOAT3 closest = ClosestPointOnBox(box.box, sphere.box.center);
    const XMFLOAT3 delta = Subtract(sphere.box.center, closest);
    return LengthSq(delta) <= sphere.sphereRadius * sphere.sphereRadius;
}

bool ShapesIntersect(const WorldShape &a, const WorldShape &b) {
    if (a.collider.shape == ColliderShape::Sphere &&
        b.collider.shape == ColliderShape::Sphere) {
        const XMFLOAT3 centerDelta = Subtract(a.box.center, b.box.center);
        const float radius = a.sphereRadius + b.sphereRadius;
        return LengthSq(centerDelta) <= radius * radius;
    }

    if (a.collider.shape == ColliderShape::Sphere) {
        return SphereIntersectsBox(a, b);
    }
    if (b.collider.shape == ColliderShape::Sphere) {
        return SphereIntersectsBox(b, a);
    }

    return BoxesIntersect(a.box, b.box);
}

BroadPhaseBounds MakeBounds(const WorldShape &shape) {
    if (shape.collider.shape == ColliderShape::Sphere) {
        return {{shape.box.center.x - shape.sphereRadius,
                 shape.box.center.y - shape.sphereRadius,
                 shape.box.center.z - shape.sphereRadius},
                {shape.box.center.x + shape.sphereRadius,
                 shape.box.center.y + shape.sphereRadius,
                 shape.box.center.z + shape.sphereRadius}};
    }

    const XMFLOAT3 extent{
        std::abs(shape.box.halfAxes[0].x) +
            std::abs(shape.box.halfAxes[1].x) +
            std::abs(shape.box.halfAxes[2].x),
        std::abs(shape.box.halfAxes[0].y) +
            std::abs(shape.box.halfAxes[1].y) +
            std::abs(shape.box.halfAxes[2].y),
        std::abs(shape.box.halfAxes[0].z) +
            std::abs(shape.box.halfAxes[1].z) +
            std::abs(shape.box.halfAxes[2].z),
    };

    return {{shape.box.center.x - extent.x, shape.box.center.y - extent.y,
             shape.box.center.z - extent.z},
            {shape.box.center.x + extent.x, shape.box.center.y + extent.y,
             shape.box.center.z + extent.z}};
}

bool BoundsOverlap(const BroadPhaseBounds &a, const BroadPhaseBounds &b) {
    return a.min.x <= b.max.x && a.max.x >= b.min.x &&
           a.min.y <= b.max.y && a.max.y >= b.min.y &&
           a.min.z <= b.max.z && a.max.z >= b.min.z;
}

int CellIndex(float value, float cellSize) {
    return static_cast<int>(std::floor(value / cellSize));
}

} // namespace

void CollisionSystem::Update(World &world) {
    events_.clear();
    std::unordered_set<CollisionPair, CollisionPairHash> currentPairs;
    std::vector<CollisionCandidate> candidates;

    world.View<Transform, Collider>(
        [&world, &candidates](Entity entity, Transform &transform,
                              Collider &collider) {
            XMMATRIX matrix = ComposeMatrix(transform);
            if (const WorldTransformMatrix *worldMatrix =
                    world.TryGet<WorldTransformMatrix>(entity)) {
                matrix = XMLoadFloat4x4(&worldMatrix->matrix);
            }
            WorldShape shape = MakeWorldShape(matrix, collider);
            candidates.push_back(
                {entity, collider, shape, MakeBounds(shape)});
        });

    std::unordered_map<GridCell, std::vector<size_t>, GridCellHash> grid;
    for (size_t i = 0; i < candidates.size(); ++i) {
        const BroadPhaseBounds &bounds = candidates[i].bounds;
        const int minX = CellIndex(bounds.min.x, broadPhaseCellSize_);
        const int minY = CellIndex(bounds.min.y, broadPhaseCellSize_);
        const int minZ = CellIndex(bounds.min.z, broadPhaseCellSize_);
        const int maxX = CellIndex(bounds.max.x, broadPhaseCellSize_);
        const int maxY = CellIndex(bounds.max.y, broadPhaseCellSize_);
        const int maxZ = CellIndex(bounds.max.z, broadPhaseCellSize_);

        for (int z = minZ; z <= maxZ; ++z) {
            for (int y = minY; y <= maxY; ++y) {
                for (int x = minX; x <= maxX; ++x) {
                    grid[{x, y, z}].push_back(i);
                }
            }
        }
    }

    std::unordered_set<CollisionPair, CollisionPairHash> candidatePairs;
    for (const auto &[_, cellCandidates] : grid) {
        for (size_t i = 0; i < cellCandidates.size(); ++i) {
            for (size_t j = i + 1; j < cellCandidates.size(); ++j) {
                const CollisionCandidate &a = candidates[cellCandidates[i]];
                const CollisionCandidate &b = candidates[cellCandidates[j]];
                candidatePairs.insert(MakePair(a.entity, b.entity));
            }
        }
    }

    std::unordered_map<Entity, size_t> indexByEntity;
    indexByEntity.reserve(candidates.size());
    for (size_t i = 0; i < candidates.size(); ++i) {
        indexByEntity[candidates[i].entity] = i;
    }

    for (const CollisionPair &pair : candidatePairs) {
        const auto aIt = indexByEntity.find(pair.a);
        const auto bIt = indexByEntity.find(pair.b);
        if (aIt == indexByEntity.end() || bIt == indexByEntity.end()) {
            continue;
        }

        const CollisionCandidate &a = candidates[aIt->second];
        const CollisionCandidate &b = candidates[bIt->second];
        if (!CanCollide(a.collider, b.collider) ||
            !BoundsOverlap(a.bounds, b.bounds)) {
            continue;
        }

        if (ShapesIntersect(a.shape, b.shape)) {
            currentPairs.insert(pair);
            const CollisionEventType type =
                previousPairs_.find(pair) == previousPairs_.end()
                    ? CollisionEventType::Enter
                    : CollisionEventType::Stay;
            events_.push_back({pair.a, pair.b, type});
        }
    }

    for (const CollisionPair &pair : previousPairs_) {
        if (!IsPairTrackable(world, pair)) {
            continue;
        }
        if (currentPairs.find(pair) == currentPairs.end()) {
            events_.push_back({pair.a, pair.b, CollisionEventType::Exit});
        }
    }

    previousPairs_ = std::move(currentPairs);
}

const std::vector<CollisionEvent> &CollisionSystem::Events() const {
    return events_;
}

void CollisionSystem::SetBroadPhaseCellSize(float cellSize) {
    broadPhaseCellSize_ = (std::max)(cellSize, 0.001f);
}

CollisionSystem::CollisionPair CollisionSystem::MakePair(Entity a,
                                                         Entity b) const {
    if (b < a) {
        return {b, a};
    }
    return {a, b};
}

bool CollisionSystem::IsPairTrackable(const World &world,
                                      const CollisionPair &pair) const {
    return world.IsAlive(pair.a) && world.IsAlive(pair.b) &&
           world.Has<Transform>(pair.a) && world.Has<Transform>(pair.b) &&
           world.Has<Collider>(pair.a) && world.Has<Collider>(pair.b);
}

bool CollisionSystem::CanCollide(const Collider &a, const Collider &b) const {
    return (a.mask & b.layer) != 0 && (b.mask & a.layer) != 0;
}
