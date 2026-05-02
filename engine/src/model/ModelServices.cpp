#include "ModelServices.h"
#include "DirectXCommon.h"
#include "ModelAssets.h"
#include "ModelRenderer.h"
#include "SkeletonDebugRenderer.h"
#include "SrvManager.h"
#include "TextureManager.h"
#include <stdexcept>

ModelServices::ModelServices()
    : assets_(std::make_unique<ModelAssets>()),
      renderer_(std::make_unique<ModelRenderer>()),
      skeletonDebug_(std::make_unique<SkeletonDebugRenderer>()) {}

ModelServices::~ModelServices() { ReleaseRenderResources(); }

void ModelServices::Initialize(DirectXCommon *dxCommon, SrvManager *srvManager,
                               TextureManager *textureManager) {
    if (!dxCommon || !srvManager || !textureManager) {
        throw std::invalid_argument(
            "ModelServices::Initialize requires valid managers");
    }

    ReleaseRenderResources();

    assets_->Initialize(dxCommon, textureManager);
    renderer_->Initialize(dxCommon, srvManager, assets_->GetMeshManager(),
                          textureManager, assets_->GetMaterialManager());
    skeletonDebug_->Initialize(dxCommon);
}

bool ModelServices::PrepareModel(uint32_t modelId) {
    Model *model = assets_->GetModel(modelId);
    if (!model) {
        return false;
    }

    renderer_->CreateSkinClusters(*model);
    renderer_->UpdateSkinClusters(*model);
    return true;
}

void ModelServices::ReleaseRenderResources() {
    if (!assets_ || !renderer_) {
        return;
    }

    for (Model &model : assets_->Models()) {
        renderer_->ReleaseSkinClusters(model);
    }
}
