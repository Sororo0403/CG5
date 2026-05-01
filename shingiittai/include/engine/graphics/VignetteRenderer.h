#pragma once
#include <DirectXMath.h>

class DirectXCommon;

struct VignetteParams {
    DirectX::XMFLOAT3 color = {0.0f, 0.0f, 0.0f};
    float intensity = 0.0f;
    float innerRadius = 0.0f;
    float power = 1.0f;
    float roundness = 1.0f;
};

class VignetteRenderer {
  public:
    void Initialize(DirectXCommon *) {}
    void Draw(const VignetteParams &) {}
};
