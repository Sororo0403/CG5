#pragma once

class DirectXCommon;
class SrvManager;
struct PostEffectPassContext;

class IPostEffectPass {
  public:
    virtual ~IPostEffectPass() = default;
    virtual void Initialize(DirectXCommon *dxCommon, SrvManager *srvManager,
                            int width, int height) = 0;
    virtual void Resize(int width, int height) = 0;
    virtual void Update(float deltaTime) = 0;
    virtual void Render(PostEffectPassContext &context) const = 0;
};
