#pragma once
#include "Component.h"
#include "Enemy.h"
#include "Player.h"

#include <DirectXMath.h>
#include <functional>

class Input;

class PlayerControllerComponent : public Component {
  public:
    using LookTargetProvider = std::function<DirectX::XMFLOAT3()>;
    using CameraYawProvider = std::function<float()>;

    PlayerControllerComponent(Player *player, Input *input);

    void SetInput(Input *input) { input_ = input; }
    void SetLookTargetProvider(LookTargetProvider provider);
    void SetCameraYawProvider(CameraYawProvider provider);

    void Update(float deltaTime) override;
    PlayerCombatObservation BuildCombatObservation() const;

  private:
    Player *player_ = nullptr;
    Input *input_ = nullptr;
    LookTargetProvider lookTargetProvider_{};
    CameraYawProvider cameraYawProvider_{};
};
