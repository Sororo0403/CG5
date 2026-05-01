#include "posteffect/pass/EdgePass.h"
#include "posteffect/PostEffectPassContext.h"

void EdgePass::Initialize(DirectXCommon *dxCommon, SrvManager *srvManager,
                          int width, int height) {
    (void)dxCommon;
    (void)srvManager;
    (void)width;
    (void)height;
}

void EdgePass::Resize(int width, int height) {
    (void)width;
    (void)height;
}

void EdgePass::Update(float deltaTime) { (void)deltaTime; }

void EdgePass::Render(PostEffectPassContext &context) const {
    context.constants.edgeMode = static_cast<int32_t>(mode_);
    context.constants.luminanceEdgeThreshold = luminanceThreshold_;
    context.constants.depthEdgeThreshold = depthThreshold_;
    context.constants.nearZ = nearZ_;
    context.constants.farZ = farZ_;
}

void EdgePass::SetMode(Mode mode) { mode_ = mode; }

void EdgePass::SetLuminanceThreshold(float threshold) {
    luminanceThreshold_ = threshold;
}

void EdgePass::SetDepthThreshold(float threshold) { depthThreshold_ = threshold; }

void EdgePass::SetDepthParameters(float nearZ, float farZ) {
    nearZ_ = nearZ;
    farZ_ = farZ;
}
