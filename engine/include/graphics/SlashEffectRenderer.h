#pragma once
#include <DirectXMath.h>

class Camera;
class DirectXCommon;
class SrvManager;
class TextureManager;

class [[deprecated("Use EffectSystem with EffectPreset::SlashArc instead.")]]
SlashEffectRenderer {
  public:
    void Initialize(DirectXCommon *, SrvManager *, TextureManager *) {
        initialized_ = true;
    }
    void DrawEnemySlash(const Camera &, const DirectX::XMFLOAT3 &,
                        const DirectX::XMFLOAT3 &, float, float, bool);

  private:
    bool initialized_ = false;
};

inline void SlashEffectRenderer::DrawEnemySlash(
    const Camera &camera, const DirectX::XMFLOAT3 &startWorld,
    const DirectX::XMFLOAT3 &endWorld, float phaseAlpha, float actionTime,
    bool isSweep) {
    (void)camera;
    (void)startWorld;
    (void)endWorld;
    (void)phaseAlpha;
    (void)actionTime;
    (void)isSweep;
}

