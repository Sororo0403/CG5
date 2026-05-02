#include "gameobject/EnemyAIComponent.h"

#include "gameobject/GameObject.h"

#include <utility>

EnemyAIComponent::EnemyAIComponent(Enemy *enemy) : enemy_(enemy) {}

void EnemyAIComponent::SetObservationProvider(ObservationProvider provider) {
    observationProvider_ = std::move(provider);
}

void EnemyAIComponent::Update(float deltaTime) {
    if (enemy_ == nullptr) {
        return;
    }

    if (GameObject *owner = GetOwner()) {
        enemy_->SetTransform(owner->GetTransform());
    }

    if (!frozen_) {
        PlayerCombatObservation observation{};
        if (observationProvider_) {
            observation = observationProvider_();
        }
        enemy_->Update(observation, deltaTime);
    }

    if (GameObject *owner = GetOwner()) {
        owner->GetTransformComponent().SetTransform(enemy_->GetTransform());
    }
}
