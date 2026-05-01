#pragma once
#include "Camera.h"
#include "Enemy.h"

class IntroCameraController {
  public:
    float GetFovDeg() const { return fovDeg_; }
    float GetFovLerpSpeed() const { return fovLerpSpeed_; }
    float GetLookAtHeight() const { return lookAtHeight_; }
    float GetPushIn() const { return pushIn_; }
    float GetLookAtEnemyWeight() const { return lookAtEnemyWeight_; }

    void Apply(Camera &camera, float &cameraYaw, const Enemy &enemy) const;

  private:
    float fovDeg_ = 64.0f;
    float fovLerpSpeed_ = 3.4f;
    float pushIn_ = 1.55f;
    float lookAtEnemyWeight_ = 0.90f;
    float lookAtHeight_ = 1.55f;
};
