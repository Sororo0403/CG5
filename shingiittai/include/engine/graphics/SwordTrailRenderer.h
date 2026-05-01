#pragma once
#include <DirectXMath.h>
#include <cstdint>

class Camera;
class DirectXCommon;
class SrvManager;
class TextureManager;

class SwordTrailRenderer {
  public:
    void Initialize(DirectXCommon *, SrvManager *, TextureManager *, uint32_t) {}
    void SetLifeTime(float) {}
    void SetEnabled(bool enabled) { enabled_ = enabled; }
    void BeginFrame(float) {}
    void AddPoint(const DirectX::XMFLOAT3 &, const DirectX::XMFLOAT3 &, float) {}
    void EndFrame() {}
    void Draw(const Camera &) {}

  private:
    bool enabled_ = true;
};
