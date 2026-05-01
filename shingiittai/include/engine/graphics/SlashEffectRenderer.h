#pragma once
#include <DirectXMath.h>

class Camera;
class DirectXCommon;
class SrvManager;
class TextureManager;

class SlashEffectRenderer {
  public:
    void Initialize(DirectXCommon *, SrvManager *, TextureManager *) {}
    void DrawEnemySlash(const Camera &, const DirectX::XMFLOAT3 &,
                        const DirectX::XMFLOAT3 &, float, float, bool) {}
};
