#include "PlayerEntity.h"

#include "Collider.h"
#include "InputComponent.h"
#include "MeshRenderer.h"
#include "Tags.h"
#include "Transform.h"
#include "Velocity.h"
#include "World.h"

namespace AppEntities {

Entity CreatePlayer(World &world, uint32_t modelId) {
    const Entity entity = world.CreateEntity();

    Transform transform;
    transform.position = {0.0f, 0.0f, -4.0f};
    world.Add<Transform>(entity, transform);

    world.Add<Velocity>(entity);
    world.Add<InputComponent>(entity);
    world.Add<PlayerTag>(entity);
    world.Add<MeshRenderer>(entity, MeshRenderer{modelId, true});

    Collider collider;
    collider.shape = ColliderShape::OBB;
    collider.center = {0.0f, 1.0f, 0.0f};
    collider.halfSize = {0.5f, 1.0f, 0.5f};
    collider.layer = 1u << 0;
    collider.mask = 1u << 1;
    world.Add<Collider>(entity, collider);

    return entity;
}

} // namespace AppEntities
