#pragma once

class Enemy;
class EnemyAnimationController;
class Input;
class ModelManager;

class GameFlowController {
  public:
    enum class Phase {
        Demo,
        Intro,
        Battle,
    };

    struct UpdateResult {
        bool startedThisFrame = false;
        bool nonBattleFrame = false;
    };

    void Reset();
    UpdateResult Update(Input *input, Enemy &enemy,
                        EnemyAnimationController &enemyAnimation,
                        ModelManager *modelManager);

    Phase GetPhase() const { return phase_; }
    bool IsDemo() const { return phase_ == Phase::Demo; }
    bool IsIntro(const Enemy &enemy) const;
    bool IsBattle() const { return phase_ == Phase::Battle; }
    bool ShouldShowDemoIndicator() const { return IsDemo(); }

  private:
    void EnsureDemoIntroSkipped(Enemy &enemy,
                                EnemyAnimationController &enemyAnimation,
                                ModelManager *modelManager);
    void StartIntro(Enemy &enemy, EnemyAnimationController &enemyAnimation,
                    ModelManager *modelManager);

    Phase phase_ = Phase::Demo;
    bool demoIntroSkipped_ = false;
};
