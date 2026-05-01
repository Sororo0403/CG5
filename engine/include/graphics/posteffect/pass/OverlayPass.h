#pragma once
#include <DirectXMath.h>

class Camera;
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

enum class OverlayEffectType {
    CounterVignette,
    DemoPlayIndicator,
    WarpRingStart,
    WarpRingEnd,
    ElectricRing,
};

class OverlayPass {
  public:
    void Initialize(DirectXCommon *dxCommon, SrvManager *srvManager, int width,
                    int height);
    void Resize(int width, int height);
    void Request(OverlayEffectType type);
    void Request(OverlayEffectType type, const DirectX::XMFLOAT3 &worldPosition);
    void SetCounterVignetteActive(bool active);
    void SetDemoPlayIndicatorVisible(bool visible);
    void Update(float deltaTime, const Camera &camera, int width, int height);
    void Render() const;
    const ElectricRingParamGPU &GetElectricRingParam() const {
        return electricRingParam_;
    }

  private:
    struct ActiveElectricRing {
        bool active = false;
        DirectX::XMFLOAT3 worldPos = {0.0f, 0.0f, 0.0f};
        float time = 0.0f;
        float lifeTime = 0.0f;
        float startRadius = 0.02f;
        float endRadius = 0.28f;
        float ringWidth = 0.015f;
        float distortionWidth = 0.045f;
        float distortionStrength = 0.018f;
        float swirlStrength = 0.006f;
        float cloudScale = 3.5f;
        float cloudIntensity = 1.4f;
        float brightness = 2.4f;
        float haloIntensity = 1.0f;
    };

    int width_ = 1;
    int height_ = 1;
    bool counterVignetteRequested_ = false;
    bool demoPlayIndicatorVisible_ = false;
    float counterVignetteAlpha_ = 0.0f;
    float counterVignetteFadeSpeed_ = 8.0f;
    float demoPlayEffectTime_ = 0.0f;
    ActiveElectricRing activeElectricRing_{};
    ElectricRingParamGPU electricRingParam_{};
};
