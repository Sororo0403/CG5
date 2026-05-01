#pragma once
#include "posteffect/IPostEffectPass.h"
#include <cstdint>

enum class NoiseMode : int32_t {
    None = 0,
    GrayscaleNoise = 1,
    OverlayNoise = 2,
};

class NoisePass : public IPostEffectPass {
  public:
    using Mode = NoiseMode;

    void Initialize(DirectXCommon *dxCommon, SrvManager *srvManager, int width,
                    int height) override;
    void Resize(int width, int height) override;
    void Update(float deltaTime) override;
    void Render(PostEffectPassContext &context) const override;

    void SetMode(Mode mode);
    Mode GetMode() const { return mode_; }
    void SetStrength(float strength);
    float GetStrength() const { return strength_; }
    void SetScale(float scale);
    float GetScale() const { return scale_; }
    void SetTime(float time);

  private:
    Mode mode_ = Mode::None;
    float strength_ = 0.0f;
    float scale_ = 240.0f;
    float time_ = 0.0f;
};
