#pragma once
#include "Enemy.h"
#include <cstdint>
#include <string>

struct Model;
class ModelManager;

class EnemyAnimationController {
  public:
    void Initialize(uint32_t modelId);

    void ResetIntro();

    void Sync(ModelManager *modelManager, const Enemy &enemy);
    void SetFrozen(ModelManager *modelManager, bool frozen);
    void Update(ModelManager *modelManager, float deltaTime);

  private:
    bool HasAnimation(const Model *model,
                      const std::string &animationName) const;
    std::string PickIntroAnimation(const Model *model, const Enemy &enemy,
                                   bool &outLoop) const;
    std::string PickAnimation(const Model *model, const Enemy &enemy,
                              bool &outLoop) const;

    uint32_t modelId_ = 0;

    std::string currentAnimation_{};
    bool currentLoop_ = true;

    bool introAnimationStarted_ = false;
    IntroPhase introPhase_ = IntroPhase::SecondSlash;

    bool frozen_ = false;
};
