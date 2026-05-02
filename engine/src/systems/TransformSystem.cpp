#include "TransformSystem.h"

#include "Transform.h"
#include "TransformHierarchy.h"
#include "World.h"

#include <DirectXMath.h>
#include <functional>
#include <unordered_set>

using namespace DirectX;

namespace {

XMMATRIX ComposeMatrix(const Transform &transform) {
    const XMVECTOR rotation =
        XMQuaternionNormalize(XMLoadFloat4(&transform.rotation));
    return XMMatrixScaling(transform.scale.x, transform.scale.y,
                           transform.scale.z) *
           XMMatrixRotationQuaternion(rotation) *
           XMMatrixTranslation(transform.position.x, transform.position.y,
                               transform.position.z);
}

Transform DecomposeMatrix(const XMMATRIX &matrix) {
    XMVECTOR scale{};
    XMVECTOR rotation{};
    XMVECTOR translation{};
    Transform transform{};

    if (XMMatrixDecompose(&scale, &rotation, &translation, matrix)) {
        XMStoreFloat3(&transform.scale, scale);
        XMStoreFloat4(&transform.rotation, XMQuaternionNormalize(rotation));
        XMStoreFloat3(&transform.position, translation);
    }

    return transform;
}

} // namespace

void TransformSystem::Update(World &world) {
    std::unordered_set<Entity> resolved;
    std::unordered_set<Entity> resolving;

    std::function<bool(Entity)> resolve = [&](Entity entity) {
        if (resolved.find(entity) != resolved.end()) {
            return true;
        }
        if (resolving.find(entity) != resolving.end()) {
            return false;
        }

        LocalTransform *local = world.TryGet<LocalTransform>(entity);
        Transform *worldTransform = world.TryGet<Transform>(entity);
        if (!local || !worldTransform) {
            resolved.insert(entity);
            return true;
        }

        resolving.insert(entity);

        XMMATRIX worldMatrix = ComposeMatrix(local->transform);
        ParentComponent *parentComponent =
            world.TryGet<ParentComponent>(entity);
        if (parentComponent && world.IsAlive(parentComponent->parent) &&
            parentComponent->parent != entity) {
            Entity parent = parentComponent->parent;
            if (world.Has<LocalTransform>(parent) && world.Has<Transform>(parent)) {
                if (!resolve(parent)) {
                    resolving.erase(entity);
                    *worldTransform = local->transform;
                    resolved.insert(entity);
                    return false;
                }
            }

            if (const Transform *parentTransform = world.TryGet<Transform>(parent)) {
                worldMatrix = worldMatrix * ComposeMatrix(*parentTransform);
            }
        }

        *worldTransform = DecomposeMatrix(worldMatrix);
        resolving.erase(entity);
        resolved.insert(entity);
        return true;
    };

    world.View<LocalTransform, Transform>(
        [&resolve](Entity entity, LocalTransform &, Transform &) {
            resolve(entity);
        });
}
