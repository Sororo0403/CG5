#include "posteffect/pass/VignettePass.h"
#include "posteffect/PostEffectPassContext.h"

void VignettePass::Initialize(DirectXCommon *dxCommon, SrvManager *srvManager,
                              int width, int height) {
    (void)dxCommon;
    (void)srvManager;
    (void)width;
    (void)height;
}

void VignettePass::Resize(int width, int height) {
    (void)width;
    (void)height;
}

void VignettePass::Update(float deltaTime) { (void)deltaTime; }

void VignettePass::Render(PostEffectPassContext &context) const {
    context.constants.enableVignetting = enabled_ ? 1 : 0;
}

void VignettePass::SetEnabled(bool enabled) { enabled_ = enabled; }
