#pragma once
#include <DirectXMath.h>
#include <cstdint>

class Camera;
class DirectXCommon;
class SrvManager;
class TextureManager;

class GpuSlashParticleSystem {
  public:
    void Initialize(DirectXCommon *, SrvManager *, TextureManager *, uint32_t) {}
    void EmitSlashBurst(const DirectX::XMFLOAT3 &, const DirectX::XMFLOAT3 &,
                        uint32_t, float, bool) {}
    void Render(const Camera &, float) {}
};
