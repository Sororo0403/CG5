#include "posteffect/pass/ColorGradingPass.h"
#include "posteffect/PostEffectPassContext.h"

void ColorGradingPass::Initialize(DirectXCommon *dxCommon,
                                  SrvManager *srvManager, int width,
                                  int height) {
    (void)dxCommon;
    (void)srvManager;
    (void)width;
    (void)height;
}

void ColorGradingPass::Resize(int width, int height) {
    (void)width;
    (void)height;
}

void ColorGradingPass::Update(float deltaTime) { (void)deltaTime; }

void ColorGradingPass::Render(PostEffectPassContext &context) const {
    context.constants.colorMode = static_cast<int32_t>(mode_);
}

void ColorGradingPass::SetMode(Mode mode) { mode_ = mode; }
