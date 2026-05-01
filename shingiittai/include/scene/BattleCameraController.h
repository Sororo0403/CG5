#pragma once
#include "Camera.h"
#include "Enemy.h"
#include "IntroCameraController.h"
#include "Player.h"
#include <DirectXMath.h>

class Input;

class BattleCameraController {
  public:
    void Initialize(float aspect);
    void ResetForBattle(const DirectX::XMFLOAT3 &playerPos,
                        const DirectX::XMFLOAT3 &enemyPos);
    void Update(Input *input, float deltaTime, const Player &player,
                const Enemy &enemy);
    void Refresh(float deltaTime, const Player &player, const Enemy &enemy);

    const Camera *GetCurrentCamera() const { return &camera_; }
    float GetYaw() const { return cameraYaw_; }

  private:
    void UpdateBattleCamera(float deltaTime, const Player &player,
                            const Enemy &enemy);

    Camera camera_;
    IntroCameraController introCamera_;

    float cameraYaw_ = 0.0f;
    float cameraPitch_ = 0.22f;
    float cameraDistance_ = 6.4f;
    float cameraHeight_ = 2.3f;
    float cameraSideOffset_ = 0.10f;
    float cameraLookHeight_ = 1.55f;
    float cameraLookAhead_ = 2.7f;

    bool isLockOn_ = false;
    float lockOnAssistStrength_ = 3.6f;
    float lockOnAssistMaxStep_ = 4.8f;
    float lockOnLookPlayerWeight_ = 0.48f;
    float lockOnLookEnemyWeight_ = 0.52f;
    float lockOnDistanceMin_ = 2.5f;
    float lockOnDistanceMax_ = 11.0f;
    float lockOnOrbitRadius_ = 6.8f;
    float lockOnOrbitHeight_ = 2.4f;
    float lockOnOrbitSideBias_ = 0.05f;
    float lockOnOrbitLerpSpeed_ = 7.5f;
    float lockOnOrbitPullBackMax_ = 2.3f;
    DirectX::XMFLOAT3 lockOnOrbitCameraPos_ = {0.0f, 0.0f, 0.0f};
    float lockOnLookAtLerpSpeed_ = 9.0f;
    DirectX::XMFLOAT3 lockOnLookAt_ = {0.0f, 0.0f, 0.0f};

    float warpStartAssistStrength_ = 4.5f;
    float warpStartAssistMaxStep_ = 7.0f;
    float warpEndAssistStrength_ = 6.0f;
    float warpEndAssistMaxStep_ = 10.0f;

    float currentFovDeg_ = 74.0f;
    float targetFovDeg_ = 74.0f;
    float normalFovDeg_ = 74.0f;
    float lockOnFovDeg_ = 78.0f;
    float warpFovDeg_ = 79.0f;
    float phaseTransitionFovDeg_ = 68.0f;
    float fovLerpSpeed_ = 6.5f;
    float phaseTransitionFovLerpSpeed_ = 5.5f;
    float phaseTransitionLookAtEnemyWeight_ = 0.82f;
    float phaseTransitionLookAtHeight_ = 1.45f;
    float phaseTransitionPushIn_ = 0.85f;
};
