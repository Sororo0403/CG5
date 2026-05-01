#pragma once
#include <d3d12.h>
#include <dxgi.h>

struct MagnetVec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct MagnetVec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

struct MagnetMatrix4x4 {
    float m[4][4] = {};
};

struct MagnetismicInstance {
    MagnetVec3 position{};
    float time = 0.0f;
    float size = 1.0f;
    float intensity = 1.0f;
    float alpha = 1.0f;
    float swirlA = 0.0f;
    float swirlB = 0.0f;
    float noiseScale = 1.0f;
    float stepScale = 1.0f;
    MagnetVec4 colorA{};
    MagnetVec4 colorB{};
    float brightness = 1.0f;
    float distFade = 0.0f;
    float innerBoost = 0.0f;
};

class [[deprecated("Use EffectSystem with EffectPreset::MagneticField instead.")]]
MagnetismicRenderer {
  public:
    void Initialize(ID3D12Device *, DXGI_FORMAT, DXGI_FORMAT, const wchar_t *,
                    const wchar_t *) {
        initialized_ = true;
        elapsedTime_ = 0.0f;
    }
    void BeginFrame(float deltaTime) { elapsedTime_ += deltaTime; }
    void Draw(ID3D12GraphicsCommandList *, const MagnetismicInstance &instance,
              const MagnetMatrix4x4 &viewProj, const MagnetVec3 &,
              const MagnetVec3 &) {
        (void)instance;
        (void)viewProj;
    }

  private:
    bool initialized_ = false;
    float elapsedTime_ = 0.0f;
};

