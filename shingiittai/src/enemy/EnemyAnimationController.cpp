#include "EnemyAnimationController.h"
#include "Model.h"
#include "ModelManager.h"

namespace {

const std::string kBossAnimIdle = "Action";
const std::string kBossAnimMove = "Action.001";
const std::string kBossAnimSweep =
    "\xE6\xA8\xAA\xE8\x96\x99\xE3\x81\x8E\xE6\x89\x95\xE3\x81\x84";
const std::string kBossAnimWave =
    "\xE6\xB3\xA2\xE7\x8A\xB6\xE6\x94\xBB\xE6\x92\x83";
const std::string kBossAnimSmash =
    "\xE7\xB8\xA6\xE6\x8C\xAF\xE3\x82\x8A\xE4\xB8\x8B\xE3\x82\x8D\xE3\x81\x97";

} // namespace

void EnemyAnimationController::Initialize(uint32_t modelId) {
    modelId_ = modelId;
    currentAnimation_.clear();
    currentLoop_ = true;
    introAnimationStarted_ = false;
    introPhase_ = IntroPhase::SecondSlash;
    frozen_ = false;
}

void EnemyAnimationController::ResetIntro() {
    currentAnimation_.clear();
    currentLoop_ = true;
    introAnimationStarted_ = false;
    introPhase_ = IntroPhase::SecondSlash;
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

    if (enemy.IsIntroActive()) {
        bool introLoop = false;
        std::string introAnimation =
            PickIntroAnimation(enemyModel, enemy, introLoop);
        if (!introAnimation.empty()) {
            IntroPhase introPhase = enemy.GetIntroPhase();
            const bool forceRestart =
                (introPhase_ != introPhase &&
                 (introPhase == IntroPhase::SecondSlash ||
                  introPhase == IntroPhase::SpinSlash));
            if (!introAnimationStarted_ || forceRestart ||
                currentAnimation_ != introAnimation ||
                currentLoop_ != introLoop) {
                modelManager->PlayAnimation(modelId_, introAnimation,
                                            introLoop);
                modelManager->UpdateAnimation(modelId_, 0.0f);
                currentAnimation_ = introAnimation;
                currentLoop_ = introLoop;
                introAnimationStarted_ = true;
            }
            introPhase_ = introPhase;
            return;
        }
    } else {
        introAnimationStarted_ = false;
        introPhase_ = IntroPhase::SecondSlash;
    }

    if (currentAnimation_ == nextAnimation && currentLoop_ == shouldLoop) {
        return;
    }

    modelManager->PlayAnimation(modelId_, nextAnimation, shouldLoop);
    modelManager->UpdateAnimation(modelId_, 0.0f);
    currentAnimation_ = nextAnimation;
    currentLoop_ = shouldLoop;
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
        if (HasAnimation(enemyModel, kBossAnimIdle)) {
            modelManager->PlayAnimation(modelId_, kBossAnimIdle, true);
            modelManager->UpdateAnimation(modelId_, 0.0f);
            currentAnimation_ = kBossAnimIdle;
            currentLoop_ = true;
        }
        enemyModel->isPlaying = false;
        enemyModel->animationTime = 0.0f;
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

    modelManager->UpdateAnimation(modelId_, deltaTime);
}

bool EnemyAnimationController::HasAnimation(
    const Model *model, const std::string &animationName) const {
    if (model == nullptr) {
        return false;
    }

    return model->animations.find(animationName) != model->animations.end();
}

std::string EnemyAnimationController::PickIntroAnimation(
    const Model *model, const Enemy &enemy, bool &outLoop) const {
    outLoop = false;

    if (model == nullptr || model->animations.empty()) {
        return {};
    }

    switch (enemy.GetIntroPhase()) {
    case IntroPhase::SecondSlash:
        if (HasAnimation(model, kBossAnimSmash)) {
            return kBossAnimSmash;
        }
        if (HasAnimation(model, kBossAnimSweep)) {
            return kBossAnimSweep;
        }
        break;

    case IntroPhase::SpinSlash:
        if (HasAnimation(model, kBossAnimSweep)) {
            return kBossAnimSweep;
        }
        if (HasAnimation(model, kBossAnimSmash)) {
            return kBossAnimSmash;
        }
        break;

    case IntroPhase::Settle:
        if (HasAnimation(model, kBossAnimSweep)) {
            return kBossAnimSweep;
        }
        if (HasAnimation(model, kBossAnimSmash)) {
            return kBossAnimSmash;
        }
        break;
    }

    if (HasAnimation(model, kBossAnimIdle)) {
        outLoop = true;
        return kBossAnimIdle;
    }

    outLoop = true;
    return model->currentAnimation.empty() ? model->animations.begin()->first
                                           : model->currentAnimation;
}

std::string EnemyAnimationController::PickAnimation(
    const Model *model, const Enemy &enemy, bool &outLoop) const {
    outLoop = true;

    if (model == nullptr || model->animations.empty()) {
        return {};
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
        if (HasAnimation(model, kBossAnimSweep)) {
            return kBossAnimSweep;
        }
        break;

    case ActionKind::Shot:
    case ActionKind::Wave:
        outLoop = false;
        if (HasAnimation(model, kBossAnimWave)) {
            return kBossAnimWave;
        }
        break;

    case ActionKind::Warp:
    case ActionKind::Stalk:
    case ActionKind::None:
    default:
        if (HasAnimation(model, kBossAnimIdle)) {
            return kBossAnimIdle;
        }
        break;
    }

    if (HasAnimation(model, kBossAnimIdle)) {
        return kBossAnimIdle;
    }
    if (HasAnimation(model, kBossAnimMove)) {
        return kBossAnimMove;
    }

    return model->currentAnimation.empty() ? model->animations.begin()->first
                                           : model->currentAnimation;
}
