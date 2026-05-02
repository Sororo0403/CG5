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

void ModelAnimationController::InitializeDefault(
    const Model &model, AnimationComponent &animation) {
    if (model.animations.empty()) {
        return;
    }

    animation.currentAnimation = model.animations.begin()->first;
    animation.time = 0.0f;
    animation.loop = true;
    animation.playing = true;
    animation.finished = false;
}

void ModelAnimationController::Update(Model &model, float deltaTime) {
    animator_.Update(model, deltaTime);
}

void ModelAnimationController::Update(const Model &model,
                                      AnimationComponent &animation,
                                      SkeletonPoseComponent &pose,
                                      float deltaTime) {
    animator_.Update(model, animation, pose, deltaTime);
}

void ModelAnimationController::Play(Model &model,
                                    const std::string &animationName,
                                    bool loop) {
    animator_.Play(model, animationName, loop);
}

void ModelAnimationController::Play(const Model &model,
                                    AnimationComponent &animation,
                                    const std::string &animationName,
                                    bool loop) {
    animator_.Play(model, animation, animationName, loop);
}

bool ModelAnimationController::IsFinished(const Model &model) const {
    return animator_.IsFinished(model);
}

bool ModelAnimationController::IsFinished(
    const AnimationComponent &animation) const {
    return animator_.IsFinished(animation);
}
