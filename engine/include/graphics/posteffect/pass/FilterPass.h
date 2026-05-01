#pragma once
#include "posteffect/IPostEffectPass.h"
#include <cstdint>

enum class FilterMode : int32_t {
    None = 0,
    Box3x3 = 1,
    Box5x5 = 2,
    Gaussian3x3 = 3,
    GaussianBlur7x7 = 4,
};

class FilterPass : public IPostEffectPass {
  public:
    using Mode = FilterMode;

    void Initialize(DirectXCommon *dxCommon, SrvManager *srvManager, int width,
                    int height) override;
    void Resize(int width, int height) override;
    void Update(float deltaTime) override;
    void Render(PostEffectPassContext &context) const override;

    void SetMode(Mode mode);
    Mode GetMode() const { return mode_; }

  private:
    Mode mode_ = Mode::None;
};
