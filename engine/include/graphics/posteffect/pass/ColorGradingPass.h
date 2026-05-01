#pragma once
#include "posteffect/IPostEffectPass.h"
#include <cstdint>

enum class ColorGradingMode : int32_t {
    None = 0,
    Grayscale = 1,
    Sepia = 2,
};

class ColorGradingPass : public IPostEffectPass {
  public:
    using Mode = ColorGradingMode;

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
