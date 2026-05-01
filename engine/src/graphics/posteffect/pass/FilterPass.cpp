#include "posteffect/pass/FilterPass.h"
#include "posteffect/PostEffectPassContext.h"

void FilterPass::Initialize(DirectXCommon *dxCommon, SrvManager *srvManager,
                            int width, int height) {
    (void)dxCommon;
    (void)srvManager;
    (void)width;
    (void)height;
}

void FilterPass::Resize(int width, int height) {
    (void)width;
    (void)height;
}

void FilterPass::Update(float deltaTime) { (void)deltaTime; }

void FilterPass::Render(PostEffectPassContext &context) const {
    context.constants.filterMode = static_cast<int32_t>(mode_);
}

void FilterPass::SetMode(Mode mode) { mode_ = mode; }
