#pragma once
#include "Component.h"
#include "Enemy.h"

#include <functional>

class EnemyAIComponent : public Component {
  public:
    using ObservationProvider = std::function<PlayerCombatObservation()>;

    explicit EnemyAIComponent(Enemy *enemy);

    void SetObservationProvider(ObservationProvider provider);
    void SetFrozen(bool frozen) { frozen_ = frozen; }
    bool IsFrozen() const { return frozen_; }

    void Update(float deltaTime) override;

  private:
    Enemy *enemy_ = nullptr;
    bool frozen_ = false;
    ObservationProvider observationProvider_{};
};
