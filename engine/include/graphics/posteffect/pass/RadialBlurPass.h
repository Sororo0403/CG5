#pragma once
#include "posteffect/IPostEffectPass.h"
#include <cstdint>

class RadialBlurPass : public IPostEffectPass {
  public:
    void Initialize(DirectXCommon *dxCommon, SrvManager *srvManager, int width,
                    int height) override;
    void Resize(int width, int height) override;
    void Update(float deltaTime) override;
    void Render(PostEffectPassContext &context) const override;

    void SetCenter(float x, float y);
    const float *GetCenter() const { return center_; }
    void SetStrength(float strength);
    float GetStrength() const { return strength_; }
    void SetSampleCount(int32_t sampleCount);
    int32_t GetSampleCount() const { return sampleCount_; }

  private:
    float center_[2]{0.5f, 0.5f};
    float strength_ = 0.0f;
    int32_t sampleCount_ = 10;
};
