#pragma once
#include "AnimationComponents.h"
#include "Animator.h"
#include <string>

struct Model;

/// <summary>
/// Modelが保持するアニメーション状態の初期化と再生制御を担当する。
/// </summary>
class ModelAnimationController {
  public:
    void InitializeDefault(Model &model);
    void InitializeDefault(const Model &model, AnimationComponent &animation);
    void Update(Model &model, float deltaTime);
    void Update(const Model &model, AnimationComponent &animation,
                SkeletonPoseComponent &pose, float deltaTime);
    void Play(Model &model, const std::string &animationName,
              bool loop = true);
    void Play(const Model &model, AnimationComponent &animation,
              const std::string &animationName, bool loop = true);
    bool IsFinished(const Model &model) const;
    bool IsFinished(const AnimationComponent &animation) const;

  private:
    Animator animator_;
};
