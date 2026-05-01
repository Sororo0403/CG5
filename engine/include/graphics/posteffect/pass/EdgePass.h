#pragma once
#include "posteffect/IPostEffectPass.h"
#include <cstdint>

enum class EdgeMode : int32_t {
    None = 0,
    Luminance = 1,
    Depth = 2,
};

class EdgePass : public IPostEffectPass {
  public:
    using Mode = EdgeMode;

    void Initialize(DirectXCommon *dxCommon, SrvManager *srvManager, int width,
                    int height) override;
    void Resize(int width, int height) override;
    void Update(float deltaTime) override;
    void Render(PostEffectPassContext &context) const override;

    void SetMode(Mode mode);
    Mode GetMode() const { return mode_; }
    void SetLuminanceThreshold(float threshold);
    float GetLuminanceThreshold() const { return luminanceThreshold_; }
    void SetDepthThreshold(float threshold);
    float GetDepthThreshold() const { return depthThreshold_; }
    void SetDepthParameters(float nearZ, float farZ);

  private:
    Mode mode_ = Mode::None;
    float luminanceThreshold_ = 0.2f;
    float depthThreshold_ = 0.02f;
    float nearZ_ = 0.1f;
    float farZ_ = 100.0f;
};
