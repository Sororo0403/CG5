#include "ModelAnimationController.h"
#include "Model.h"

void ModelAnimationController::InitializeDefault(Model &model) {
    if (model.animations.empty()) {
        return;
    }

    model.currentAnimation = model.animations.begin()->first;
    model.animationTime = 0.0f;
    model.isLoop = true;
    model.isPlaying = true;
    model.animationFinished = false;
}

void ModelAnimationController::Update(Model &model, float deltaTime) {
    animator_.Update(model, deltaTime);
}

void ModelAnimationController::Play(Model &model,
                                    const std::string &animationName,
                                    bool loop) {
    animator_.Play(model, animationName, loop);
}

bool ModelAnimationController::IsFinished(const Model &model) const {
    return animator_.IsFinished(model);
}
