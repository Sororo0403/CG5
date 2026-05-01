#include "GameFlowController.h"
#include "Enemy.h"
#include "EnemyAnimationController.h"
#include "Input.h"

void GameFlowController::Reset() {
    phase_ = Phase::Demo;
    demoIntroSkipped_ = false;
}

GameFlowController::UpdateResult
GameFlowController::Update(Input *input, Enemy &enemy,
                           EnemyAnimationController &enemyAnimation,
                           ModelManager *modelManager) {
    UpdateResult result{};

    if (phase_ == Phase::Demo) {
        EnsureDemoIntroSkipped(enemy, enemyAnimation, modelManager);

        if (input != nullptr && input->IsKeyTrigger(DIK_SPACE)) {
            StartIntro(enemy, enemyAnimation, modelManager);
            result.startedThisFrame = true;
            result.nonBattleFrame = true;
            return result;
        }

        result.nonBattleFrame = true;
        return result;
    }

    if (phase_ == Phase::Intro) {
        if (!enemy.IsIntroActive()) {
            phase_ = Phase::Battle;
            return result;
        }

        result.nonBattleFrame = true;
        return result;
    }

    return result;
}

bool GameFlowController::IsIntro(const Enemy &enemy) const {
    return phase_ == Phase::Intro && enemy.IsIntroActive();
}

void GameFlowController::EnsureDemoIntroSkipped(
    Enemy &enemy, EnemyAnimationController &enemyAnimation,
    ModelManager *modelManager) {
    if (demoIntroSkipped_) {
        return;
    }

    enemy.SkipIntro();
    demoIntroSkipped_ = true;
    enemyAnimation.Sync(modelManager, enemy);
}

void GameFlowController::StartIntro(Enemy &enemy,
                                    EnemyAnimationController &enemyAnimation,
                                    ModelManager *modelManager) {
    phase_ = Phase::Intro;
    enemy.RestartIntro();
    enemyAnimation.ResetIntro();
    enemyAnimation.SetFrozen(modelManager, false);
    enemyAnimation.Sync(modelManager, enemy);
}
