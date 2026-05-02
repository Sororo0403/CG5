#pragma once
#include "Animator.h"
#include <string>

struct Model;

/// <summary>
/// Modelが保持するアニメーション状態の初期化と再生制御を担当する。
/// </summary>
class ModelAnimationController {
  public:
    void InitializeDefault(Model &model);
    void Update(Model &model, float deltaTime);
    void Play(Model &model, const std::string &animationName,
              bool loop = true);
    bool IsFinished(const Model &model) const;

  private:
    Animator animator_;
};
