#pragma once
#include "Camera.h"
#include "Enemy.h"

class IntroCameraController {
  public:
    void Reset();
    void Apply(Camera &camera, float deltaTime, const Enemy &enemy);

  private:
    float BlendFov(float deltaTime);

    float currentFovDeg_ = 74.0f;
    float fovDeg_ = 64.0f;
    float fovLerpSpeed_ = 3.4f;
    float lookAtHeight_ = 1.55f;
};
