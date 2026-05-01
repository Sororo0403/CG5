#pragma once
#include <DirectXMath.h>
#include <d3d12.h>

class DirectXCommon;
class SrvManager;

struct ElectricRingParamGPU {
    DirectX::XMFLOAT2 center = {0.5f, 0.5f};
    float radius = 0.0f;
    float time = 0.0f;
    float ringWidth = 0.0f;
    float distortionWidth = 0.0f;
    float distortionStrength = 0.0f;
    float swirlStrength = 0.0f;
    float cloudScale = 0.0f;
    float cloudIntensity = 0.0f;
    float brightness = 0.0f;
    float haloIntensity = 0.0f;
    DirectX::XMFLOAT2 aspectInvAspect = {1.0f, 1.0f};
    float innerFade = 0.0f;
    float outerFade = 0.0f;
    float enabled = 0.0f;
};

class ElectricRingEffectRenderer {
  public:
    void Initialize(DirectXCommon *, SrvManager *) {}
    void DrawDistortion(const ElectricRingParamGPU &, D3D12_GPU_DESCRIPTOR_HANDLE,
                        D3D12_GPU_DESCRIPTOR_HANDLE,
                        D3D12_GPU_DESCRIPTOR_HANDLE) {}
    void DrawPlasma(const ElectricRingParamGPU &, D3D12_GPU_DESCRIPTOR_HANDLE,
                    D3D12_GPU_DESCRIPTOR_HANDLE) {}
};
