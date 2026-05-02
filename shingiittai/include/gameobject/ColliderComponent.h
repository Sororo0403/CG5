#pragma once
#include "Component.h"
#include "OBB.h"

#include <functional>

enum class ColliderLayer {
    Default,
    Player,
    Enemy,
    PlayerWeapon,
    EnemyAttack,
    Projectile
};

class ColliderComponent : public Component {
  public:
    using OBBProvider = std::function<OBB()>;

    ColliderComponent() = default;
    ColliderComponent(ColliderLayer layer, OBBProvider obbProvider);

    void SetLayer(ColliderLayer layer) { layer_ = layer; }
    ColliderLayer GetLayer() const { return layer_; }

    void SetOBBProvider(OBBProvider obbProvider);
    OBB GetOBB() const;

    void SetEnabled(bool enabled) { enabled_ = enabled; }
    bool IsEnabled() const { return enabled_; }

    bool Intersects(const ColliderComponent &other) const;
    static bool Intersects(const OBB &a, const OBB &b);

  private:
    ColliderLayer layer_ = ColliderLayer::Default;
    bool enabled_ = true;
    OBBProvider obbProvider_{};
};
