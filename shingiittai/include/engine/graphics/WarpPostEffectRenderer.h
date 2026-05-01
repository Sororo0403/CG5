#pragma once
#include <DirectXMath.h>
#include <d3d12.h>

class DirectXCommon;
class SrvManager;

struct WarpPostEffectParamGPU {
    DirectX::XMFLOAT2 center = {0.5f, 0.5f};
    float radius = 0.0f;
    float strength = 0.0f;
    DirectX::XMFLOAT2 center2 = {0.5f, 0.5f};
    float radius2 = 0.0f;
    float strength2 = 0.0f;
    float time = 0.0f;
    float enabled = 0.0f;
    DirectX::XMFLOAT2 slashStart = {0.5f, 0.5f};
    DirectX::XMFLOAT2 slashEnd = {0.5f, 0.5f};
    float slashThickness = 0.0f;
    float slashStrength = 0.0f;
    float slashEnabled = 0.0f;
};

class WarpPostEffectRenderer {
  public:
    void Initialize(DirectXCommon *, SrvManager *) {}
    void Draw(const WarpPostEffectParamGPU &, D3D12_GPU_DESCRIPTOR_HANDLE) {}
};
