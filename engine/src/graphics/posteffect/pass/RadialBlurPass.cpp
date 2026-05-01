#include "posteffect/pass/RadialBlurPass.h"
#include "posteffect/PostEffectPassContext.h"
#include <algorithm>

void RadialBlurPass::Initialize(DirectXCommon *dxCommon, SrvManager *srvManager,
                                int width, int height) {
    (void)dxCommon;
    (void)srvManager;
    (void)width;
    (void)height;
}

void RadialBlurPass::Resize(int width, int height) {
    (void)width;
    (void)height;
}

void RadialBlurPass::Update(float deltaTime) { (void)deltaTime; }

void RadialBlurPass::Render(PostEffectPassContext &context) const {
    context.constants.radialBlurCenter[0] = center_[0];
    context.constants.radialBlurCenter[1] = center_[1];
    context.constants.radialBlurStrength = strength_;
    context.constants.radialBlurSampleCount = sampleCount_;
}

void RadialBlurPass::SetCenter(float x, float y) {
    center_[0] = std::clamp(x, 0.0f, 1.0f);
    center_[1] = std::clamp(y, 0.0f, 1.0f);
}

void RadialBlurPass::SetStrength(float strength) {
    strength_ = std::clamp(strength, 0.0f, 1.0f);
}

void RadialBlurPass::SetSampleCount(int32_t sampleCount) {
    sampleCount_ = std::clamp(sampleCount, 2, 32);
}
