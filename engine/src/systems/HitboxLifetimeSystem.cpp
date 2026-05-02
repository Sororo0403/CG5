#include "HitboxLifetimeSystem.h"

#include "GameplayComponents.h"
#include "World.h"
#include "WorldCommandBuffer.h"

void HitboxLifetimeSystem::Update(World &world, WorldCommandBuffer &commands,
                                  float deltaTime) {
    world.View<Hitbox, HitboxLifetime>(
        [&commands, deltaTime](Entity entity, Hitbox &hitbox,
                               HitboxLifetime &lifetime) {
            lifetime.timeRemaining -= deltaTime;
            if (lifetime.timeRemaining > 0.0f) {
                return;
            }

            hitbox.active = false;
            if (lifetime.destroyEntityWhenExpired) {
                commands.DestroyEntity(entity);
            }
        });
}
