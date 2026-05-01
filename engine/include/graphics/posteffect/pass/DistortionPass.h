#pragma once
#include <DirectXMath.h>
#include <d3d12.h>

class DirectXCommon;
class SrvManager;
class SpriteRenderer;

enum class DistortionPreset {
    WarpPortal,
    Shockwave,
    UltimateBurst,
    HeatHaze,
    WaterRipple,
};

enum class DistortionPhase { Start, Sustain, End };

struct DistortionEffectParams {
    DistortionPreset preset = DistortionPreset::WarpPortal;
    DistortionPhase phase = DistortionPhase::Start;
    bool hasOrigin = false;
    bool hasTarget = false;
    DirectX::XMFLOAT2 originScreen = {0.0f, 0.0f};
    DirectX::XMFLOAT2 targetScreen = {0.0f, 0.0f};
    float time = 0.0f;
    float intensity = 1.0f;
    float baseRadius = 100.0f;
    float jitter = 0.0f;
    float alpha = 0.0f;
    float thickness = 4.0f;
    float lineLength = 72.0f;
    float footOffset = 0.0f;
    float previewOffset = 46.0f;
};

class DistortionPass {
  public:
    void Initialize(DirectXCommon *dxCommon, SrvManager *srvManager, int width,
                    int height);
    void Resize(int width, int height);
    void Update(float deltaTime);
    void Request(const DistortionEffectParams &params);
    void Render(D3D12_GPU_DESCRIPTOR_HANDLE sourceTexture,
                D3D12_GPU_DESCRIPTOR_HANDLE depthTexture,
                SpriteRenderer *spriteRenderer) const;

  private:
    bool requested_ = false;
    DistortionEffectParams params_{};
};
