#include "posteffect/pass/NoisePass.h"
#include "posteffect/PostEffectPassContext.h"
#include <algorithm>

void NoisePass::Initialize(DirectXCommon *dxCommon, SrvManager *srvManager,
                           int width, int height) {
    (void)dxCommon;
    (void)srvManager;
    (void)width;
    (void)height;
}

void NoisePass::Resize(int width, int height) {
    (void)width;
    (void)height;
}

void NoisePass::Update(float deltaTime) { (void)deltaTime; }

void NoisePass::Render(PostEffectPassContext &context) const {
    context.constants.randomMode = static_cast<int32_t>(mode_);
    context.constants.randomStrength = strength_;
    context.constants.randomScale = scale_;
    context.constants.randomTime = time_;
}

void NoisePass::SetMode(Mode mode) { mode_ = mode; }

void NoisePass::SetStrength(float strength) {
    strength_ = std::clamp(strength, 0.0f, 1.0f);
}

void NoisePass::SetScale(float scale) {
    scale_ = std::clamp(scale, 1.0f, 4096.0f);
}

void NoisePass::SetTime(float time) { time_ = time; }
