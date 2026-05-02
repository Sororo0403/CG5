#include "gameobject/RenderComponent.h"

#include "Camera.h"
#include "ModelManager.h"
#include "gameobject/GameObject.h"

#include <utility>

RenderComponent::RenderComponent(ModelManager *modelManager, uint32_t modelId)
    : modelManager_(modelManager), modelId_(modelId) {}

RenderComponent::RenderComponent(ModelManager *modelManager, uint32_t modelId,
                                 DrawCallback drawCallback)
    : modelManager_(modelManager), modelId_(modelId),
      drawCallback_(std::move(drawCallback)) {}

void RenderComponent::SetModel(ModelManager *modelManager, uint32_t modelId) {
    modelManager_ = modelManager;
    modelId_ = modelId;
}

void RenderComponent::SetDrawCallback(DrawCallback drawCallback) {
    drawCallback_ = std::move(drawCallback);
}

void RenderComponent::Draw(const Camera &camera) {
    if (!visible_ || modelManager_ == nullptr) {
        return;
    }

    if (drawCallback_) {
        drawCallback_(modelManager_, camera);
        return;
    }

    const GameObject *owner = GetOwner();
    if (owner == nullptr) {
        return;
    }

    modelManager_->Draw(modelId_, owner->GetTransform(), camera);
}
