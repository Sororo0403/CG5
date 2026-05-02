#include "gameobject/PlayerControllerComponent.h"

#include "gameobject/GameObject.h"

#include <utility>

PlayerControllerComponent::PlayerControllerComponent(Player *player,
                                                     Input *input)
    : player_(player), input_(input) {}

void PlayerControllerComponent::SetLookTargetProvider(
    LookTargetProvider provider) {
    lookTargetProvider_ = std::move(provider);
}

void PlayerControllerComponent::SetCameraYawProvider(CameraYawProvider provider) {
    cameraYawProvider_ = std::move(provider);
}

void PlayerControllerComponent::Update(float deltaTime) {
    if (player_ == nullptr || input_ == nullptr) {
        return;
    }

    if (GameObject *owner = GetOwner()) {
        player_->SetTransform(owner->GetTransform());
    }

    DirectX::XMFLOAT3 lookTarget = player_->GetTransform().position;
    if (lookTargetProvider_) {
        lookTarget = lookTargetProvider_();
    }

    float cameraYaw = 0.0f;
    if (cameraYawProvider_) {
        cameraYaw = cameraYawProvider_();
    }

    player_->Update(input_, deltaTime, lookTarget, cameraYaw);

    if (GameObject *owner = GetOwner()) {
        owner->GetTransformComponent().SetTransform(player_->GetTransform());
    }
}

PlayerCombatObservation
PlayerControllerComponent::BuildCombatObservation() const {
    PlayerCombatObservation observation{};
    if (player_ == nullptr) {
        return observation;
    }

    const auto swordSlashStates = player_->GetSwordSlashStates();
    observation.position = player_->GetTransform().position;
    observation.velocity = player_->GetVelocity();
    observation.isGuarding = player_->IsGuarding();
    observation.isCounterStance = player_->IsCounterStance();
    observation.justCountered = player_->JustCountered();
    observation.justCounterFailed = player_->JustCounterFailed();
    observation.justCounterEarly = player_->JustCounterEarly();
    observation.justCounterLate = player_->JustCounterLate();
    observation.isAttacking = swordSlashStates[0] || swordSlashStates[1];

    switch (player_->GetCounterAxis()) {
    case SwordCounterAxis::Vertical:
        observation.counterAxis = CounterAxis::Vertical;
        break;
    case SwordCounterAxis::Horizontal:
        observation.counterAxis = CounterAxis::Horizontal;
        break;
    default:
        observation.counterAxis = CounterAxis::None;
        break;
    }

    return observation;
}
