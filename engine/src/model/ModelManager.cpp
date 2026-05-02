#include "ModelManager.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "TextureManager.h"
#include <stdexcept>

ModelManager::~ModelManager() { ReleaseModelRendererResources(); }

void ModelManager::Initialize(DirectXCommon *dxCommon, SrvManager *srvManager,
                              TextureManager *textureManager) {
    if (!dxCommon || !srvManager || !textureManager) {
        throw std::invalid_argument(
            "ModelManager::Initialize requires valid managers");
    }

    ReleaseModelRendererResources();

    assets_.Initialize(dxCommon, textureManager);

    modelRenderer_.Initialize(dxCommon, srvManager, assets_.GetMeshManager(),
                              textureManager, assets_.GetMaterialManager());
    skeletonDebugRenderer_.Initialize(dxCommon);
}

uint32_t ModelManager::Load(const DirectXCommon::UploadContext &uploadContext,
                            const std::wstring &path) {
    const uint32_t modelId = assets_.Load(uploadContext, path);
    Model *model = assets_.GetModel(modelId);
    if (!model) {
        return modelId;
    }

    modelRenderer_.CreateSkinClusters(*model);
    animation_.InitializeDefault(*model);
    animation_.Update(*model, 0.0f);
    modelRenderer_.UpdateSkinClusters(*model);

    return modelId;
}

uint32_t ModelManager::CreatePlane(uint32_t textureId,
                                   const Material &material) {
    const uint32_t modelId = assets_.CreatePlane(textureId, material);
    if (Model *model = assets_.GetModel(modelId)) {
        modelRenderer_.CreateSkinClusters(*model);
    }
    return modelId;
}

uint32_t ModelManager::CreateRing(uint32_t textureId, const Material &material,
                                  uint32_t divide, float outerRadius,
                                  float innerRadius) {
    const uint32_t modelId =
        assets_.CreateRing(textureId, material, divide, outerRadius,
                           innerRadius);
    if (Model *model = assets_.GetModel(modelId)) {
        modelRenderer_.CreateSkinClusters(*model);
    }
    return modelId;
}

uint32_t ModelManager::CreateCylinder(uint32_t textureId,
                                      const Material &material, uint32_t divide,
                                      float topRadius, float bottomRadius,
                                      float height) {
    const uint32_t modelId =
        assets_.CreateCylinder(textureId, material, divide, topRadius,
                               bottomRadius, height);
    if (Model *model = assets_.GetModel(modelId)) {
        modelRenderer_.CreateSkinClusters(*model);
    }
    return modelId;
}

void ModelManager::UpdateAnimation(uint32_t modelId, float deltaTime) {
    Model *model = assets_.GetModel(modelId);
    if (!model) {
        return;
    }

    animation_.Update(*model, deltaTime);
    modelRenderer_.UpdateSkinClusters(*model);
}

void ModelManager::PlayAnimation(uint32_t modelId,
                                 const std::string &animationName, bool loop) {
    Model *model = assets_.GetModel(modelId);
    if (!model) {
        return;
    }

    animation_.Play(*model, animationName, loop);
}

bool ModelManager::IsAnimationFinished(uint32_t modelId) const {
    const Model *model = assets_.GetModel(modelId);
    if (!model) {
        return false;
    }

    return animation_.IsFinished(*model);
}

Model *ModelManager::GetModel(uint32_t modelId) {
    return assets_.GetModel(modelId);
}

const Model *ModelManager::GetModel(uint32_t modelId) const {
    return assets_.GetModel(modelId);
}

const Material &ModelManager::GetMaterial(uint32_t materialId) const {
    return assets_.GetMaterial(materialId);
}

void ModelManager::SetMaterial(uint32_t materialId, const Material &material) {
    assets_.SetMaterial(materialId, material);
}

void ModelManager::DrawSkeleton(uint32_t modelId, const Transform &transform,
                                const Camera &camera) {
    const Model *model = assets_.GetModel(modelId);
    if (!model) {
        return;
    }

    skeletonDebugRenderer_.Draw(*model, transform, camera);
}

void ModelManager::Draw(uint32_t modelId, const Transform &transform,
                        const Camera &camera) {
    const Model *model = assets_.GetModel(modelId);
    if (!model) {
        return;
    }

    modelRenderer_.Draw(*model, transform, camera);
}

void ModelManager::ReleaseModelRendererResources() {
    for (Model &model : assets_.Models()) {
        modelRenderer_.ReleaseSkinClusters(model);
    }
}
