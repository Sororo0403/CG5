#include "GameRuleSystem.h"

#include "Tags.h"
#include "World.h"

void GameRuleSystem::Update(World &world,
                            const std::vector<CollisionEvent> &collisions,
                            float deltaTime) {
    (void)deltaTime;

    for (const CollisionEvent &collision : collisions) {
        const bool playerEnemy =
            (world.Has<PlayerTag>(collision.a) &&
             world.Has<EnemyTag>(collision.b)) ||
            (world.Has<PlayerTag>(collision.b) &&
             world.Has<EnemyTag>(collision.a));

        if (playerEnemy) {
            lastPlayerEnemyCollision_ = collision;
        }
    }
}

bool GameRuleSystem::HasPlayerEnemyCollision() const {
    return lastPlayerEnemyCollision_.a != kInvalidEntity;
}

CollisionEvent GameRuleSystem::LastPlayerEnemyCollision() const {
    return lastPlayerEnemyCollision_;
}

void GameRuleSystem::ClearFrameState() {
    lastPlayerEnemyCollision_ = {};
}
