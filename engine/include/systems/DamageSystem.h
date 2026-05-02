#pragma once
#include "GameplayComponents.h"

#include <vector>

class World;

/// <summary>
/// Hitbox/HurtboxからHealthへダメージを適用するSystem。
/// </summary>
struct DamageSystem {
    void Update(World &world, float deltaTime);
    const std::vector<DamageEvent> &Events() const { return events_; }

  private:
    std::vector<DamageEvent> events_;
};
