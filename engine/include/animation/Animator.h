#pragma once
#include "AnimationComponents.h"
#include "Model.h"

/// <summary>
/// モデルアニメーションの再生制御を担当する
/// </summary>
class Animator {
  public:
    /// <summary>
    /// アニメーション再生開始
    /// </summary>
    void Play(Model &model, const std::string &animationName, bool loop = true);
    void Play(const Model &model, AnimationComponent &animation,
              const std::string &animationName, bool loop = true);

    /// <summary>
    /// 更新処理
    /// </summary>
    void Update(Model &model, float deltaTime);
    void Update(const Model &model, AnimationComponent &animation,
                SkeletonPoseComponent &pose, float deltaTime);

    /// <summary>
    /// 再生終了したか
    /// </summary>
    bool IsFinished(const Model &model) const;
    bool IsFinished(const AnimationComponent &animation) const;

  private:
    /// <summary>
    /// モデルをバインドポーズへ戻す
    /// </summary>
    void ApplyBindPose(Model &model);
    void ApplyBindPose(const Model &model, SkeletonPoseComponent &pose);
};
