#pragma once
#include "posteffect/IPostEffectPass.h"

class VignettePass : public IPostEffectPass {
  public:
    void Initialize(DirectXCommon *dxCommon, SrvManager *srvManager, int width,
                    int height) override;
    void Resize(int width, int height) override;
    void Update(float deltaTime) override;
    void Render(PostEffectPassContext &context) const override;

    void SetEnabled(bool enabled);
    bool IsEnabled() const { return enabled_; }

  private:
    bool enabled_ = true;
};
