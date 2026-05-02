#include "EnemyEntity.h"

#include "Collider.h"
#include "MeshRenderer.h"
#include "Tags.h"
#include "Transform.h"
#include "Velocity.h"
#include "World.h"

namespace AppEntities {

Entity CreateEnemy(World &world, uint32_t modelId) {
    const Entity entity = world.CreateEntity();

    Transform transform;
    transform.position = {0.0f, 0.0f, 4.0f};
    world.Add<Transform>(entity, transform);

    world.Add<Velocity>(entity);
    world.Add<EnemyTag>(entity);
    world.Add<MeshRenderer>(entity, MeshRenderer{modelId, true});

    Collider collider;
    collider.shape = ColliderShape::OBB;
    collider.center = {0.0f, 1.0f, 0.0f};
    collider.halfSize = {0.6f, 1.2f, 0.6f};
    collider.layer = 1u << 1;
    collider.mask = 1u << 0;
    world.Add<Collider>(entity, collider);

    return entity;
}

} // namespace AppEntities
