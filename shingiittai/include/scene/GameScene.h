#pragma once
#include "BattleCameraController.h"
#include "BattleEffectDirector.h"
#include "BaseScene.h"
#include "Camera.h"
#include "CombatSystem.h"
#include "Enemy.h"
#include "EnemyAnimationController.h"
#include "LightingController.h"
#include "Player.h"
#include <DirectXMath.h>
#include <cstdint>

class GameScene : public BaseScene {
  public:
    void Initialize(const SceneContext &ctx) override;
    void Update() override;
    void Draw() override;

    const Camera *GetCurrentCamera() const { return &camera_; }

  private:
    void UpdateCamera(Input *input);
    void UpdateSceneCamera();
    void UpdateCounterVignette(float deltaTime);
    void DrawCounterVignette();

  private:
    Camera camera_;
    BattleCameraController battleCamera_;
    BattleEffectDirector battleEffects_;
    LightingController lighting_;

    Player player_;
    Enemy enemy_;
    EnemyAnimationController enemyAnimation_;
    CombatSystem combatSystem_;
    uint32_t playerModelId_ = 0;
    uint32_t enemyModelId_ = 0;

#ifdef _DEBUG
    bool dbgFreezeEnemyMotion_ = false;
#endif

    bool counterCinematicActive_ = false;
    float counterTimeScale_ = 0.05f;
};
