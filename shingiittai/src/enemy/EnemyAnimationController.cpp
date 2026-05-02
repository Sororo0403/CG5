#include "EnemyAnimationController.h"
#include "Model.h"
#include "ModelManager.h"

#include <algorithm>

namespace {

const std::string kBossAnimWarp = "テレポート";
const std::string kBossAnimShot = "弾発射";
const std::string kBossAnimSweepRight = "横薙ぎ(右から)";
const std::string kBossAnimSweep = "横薙ぎ払い";
const std::string kBossAnimWave = "波状攻撃";
const std::string kBossAnimPhaseTransition = "第二形態移行";
const std::string kBossAnimSmash = "縦振り下ろし";

} // namespace

void EnemyAnimationController::Initialize(uint32_t modelId) {
    modelId_ = modelId;
    currentAnimation_.clear();
    currentLoop_ = true;
    playbackSpeed_ = 1.0f;
    currentActionSequence_ = 0;
    frozen_ = false;
}

void EnemyAnimationController::Sync(ModelManager *modelManager,
                                    const Enemy &enemy) {
    if (modelManager == nullptr) {
        return;
    }

    Model *enemyModel = modelManager->GetModel(modelId_);
    if (enemyModel == nullptr || enemyModel->animations.empty()) {
        return;
    }

    bool shouldLoop = true;
    std::string nextAnimation =
        PickAnimation(enemyModel, enemy, shouldLoop);
    if (nextAnimation.empty()) {
        return;
    }

    const uint32_t actionSequence = enemy.GetActionSequence();
    const bool forceActionRestart =
        !shouldLoop && currentActionSequence_ != actionSequence;

    if (currentAnimation_ == nextAnimation && currentLoop_ == shouldLoop &&
        !forceActionRestart) {
        playbackSpeed_ = CalculatePlaybackSpeed(
            enemyModel, enemy, nextAnimation, shouldLoop);
        return;
    }

    modelManager->PlayAnimation(modelId_, nextAnimation, shouldLoop);
    modelManager->UpdateAnimation(modelId_, 0.0f);
    currentAnimation_ = nextAnimation;
    currentLoop_ = shouldLoop;
    currentActionSequence_ = actionSequence;
    playbackSpeed_ =
        CalculatePlaybackSpeed(enemyModel, enemy, nextAnimation, shouldLoop);
}

void EnemyAnimationController::SetFrozen(ModelManager *modelManager,
                                         bool frozen) {
    if (modelManager == nullptr) {
        frozen_ = frozen;
        return;
    }

    Model *enemyModel = modelManager->GetModel(modelId_);
    if (enemyModel == nullptr) {
        frozen_ = frozen;
        return;
    }

    if (frozen) {
        enemyModel->isPlaying = false;
        enemyModel->animationFinished = false;
        frozen_ = true;
        return;
    }

    if (frozen_ && !enemyModel->animationFinished &&
        !enemyModel->currentAnimation.empty()) {
        enemyModel->isPlaying = true;
    }
    frozen_ = false;
}

void EnemyAnimationController::Update(ModelManager *modelManager,
                                      float deltaTime) {
    if (modelManager == nullptr || frozen_) {
        return;
    }

    modelManager->UpdateAnimation(modelId_, deltaTime * playbackSpeed_);
}

bool EnemyAnimationController::HasAnimation(
    const Model *model, const std::string &animationName) const {
    if (model == nullptr) {
        return false;
    }

    return model->animations.find(animationName) != model->animations.end();
}

float EnemyAnimationController::CalculatePlaybackSpeed(
    const Model *model, const Enemy &enemy, const std::string &animationName,
    bool loop) const {
    if (model == nullptr || loop) {
        return 1.0f;
    }

    auto clipIt = model->animations.find(animationName);
    if (clipIt == model->animations.end() || clipIt->second.duration <= 0.0f) {
        return 1.0f;
    }

    const float actionRemaining = enemy.GetCurrentAnimationRemainingTime();
    if (actionRemaining <= 0.001f) {
        return 1.0f;
    }

    float clipRemaining = clipIt->second.duration;
    if (model->currentAnimation == animationName) {
        clipRemaining -= model->animationTime;
    }
    if (clipRemaining <= 0.001f) {
        return 1.0f;
    }

    return (std::clamp)(clipRemaining / actionRemaining, 0.20f, 3.0f);
}

std::string EnemyAnimationController::PickAnimation(
    const Model *model, const Enemy &enemy, bool &outLoop) const {
    outLoop = true;

    if (model == nullptr || model->animations.empty()) {
        return {};
    }

    if (enemy.IsPhaseTransitionActive() &&
        HasAnimation(model, kBossAnimPhaseTransition)) {
        outLoop = false;
        return kBossAnimPhaseTransition;
    }

    switch (enemy.GetActionKind()) {
    case ActionKind::Smash:
        outLoop = false;
        if (HasAnimation(model, kBossAnimSmash)) {
            return kBossAnimSmash;
        }
        break;

    case ActionKind::Sweep:
        outLoop = false;
        if (enemy.IsDoubleSweepSecondStage() &&
            HasAnimation(model, kBossAnimSweepRight)) {
            return kBossAnimSweepRight;
        }
        if (HasAnimation(model, kBossAnimSweep)) {
            return kBossAnimSweep;
        }
        if (HasAnimation(model, kBossAnimSweepRight)) {
            return kBossAnimSweepRight;
        }
        break;

    case ActionKind::Shot:
        outLoop = false;
        if (HasAnimation(model, kBossAnimShot)) {
            return kBossAnimShot;
        }
        break;

    case ActionKind::Wave:
        outLoop = false;
        if (HasAnimation(model, kBossAnimWave)) {
            return kBossAnimWave;
        }
        break;

    case ActionKind::Warp:
        outLoop = false;
        if (HasAnimation(model, kBossAnimWarp)) {
            return kBossAnimWarp;
        }
        break;

    case ActionKind::Stalk:
    case ActionKind::None:
    default:
        break;
    }

    outLoop = false;
    return model->currentAnimation;
}
