#pragma once
#include "BattleCameraController.h"
#include "BaseScene.h"
#include "Camera.h"
#include "CombatSystem.h"
#include "Enemy.h"
#include "EnemyAnimationController.h"
#include "EffectParticleSystem.h"
#include "IntroCameraController.h"
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
    void UpdateSceneLighting();
    void UpdateCounterVignette(float deltaTime);
    void DrawCounterVignette();
    void DrawDemoPlayIndicator();
    bool ProjectWorldToScreen(const DirectX::XMFLOAT3 &worldPos,
                              DirectX::XMFLOAT2 &outScreen) const;
    bool ComputeEnemySlashWorldEffect(DirectX::XMFLOAT3 &outStart,
                                      DirectX::XMFLOAT3 &outEnd,
                                      float &outPhaseAlpha,
                                      float &outActionTime,
                                      ActionKind &outActionKind) const;
    void UpdateEnemySlashEffects();
    void RequestEffect(EffectParticlePreset preset,
                       const DirectX::XMFLOAT3 &position);
    void DrawWarpSmokePass();
    void DrawWarpDistortionPass();

    void SpawnElectricRing(const DirectX::XMFLOAT3 &worldPos, bool isWarpEnd);
    void UpdateElectricRing();

    void UpdateEnemySwordTrail();

  private:
    Camera camera_;
    BattleCameraController battleCamera_;
    IntroCameraController introCamera_;

    Player player_;
    Enemy enemy_;
    EnemyAnimationController enemyAnimation_;
    CombatSystem combatSystem_;
    uint32_t playerModelId_ = 0;
    uint32_t enemyModelId_ = 0;

#ifdef _DEBUG
    bool dbgFreezeEnemyMotion_ = false;
#endif

    float warpDistortionRadiusPx_ = 116.0f;
    float warpDistortionThicknessPx_ = 4.0f;
    float warpDistortionLineLengthPx_ = 72.0f;
    float warpDistortionAlpha_ = 0.34f;
    float warpDistortionMoveAlphaBonus_ = 0.20f;
    float warpDistortionJitterPx_ = 16.0f;
    float warpDistortionPreviewOffsetPx_ = 46.0f;
    float warpDistortionFootOffsetPx_ = 0.0f;
    float warpSourceSmokeBloomScale_ = 1.42f;
    float warpArrivalSmokeDenseScale_ = 1.58f;
    float sceneLightTime_ = 0.0f;

    bool counterCinematicActive_ = false;
    bool hasGameStarted_ = false;
    bool demoIntroSkipped_ = false;
    float counterTimeScale_ = 0.05f;

    // Enemy slash presentation
    float enemySlashChargePreviewAlpha_ = 0.22f;
    float enemySlashActiveAlpha_ = 0.92f;
    float enemySlashRecoveryAlpha_ = 0.36f;

    // slash effect
    bool enemySlashActiveLatched_ = false;

    float enemySlashParticleEmitScale_ = 1.0f;
    uint32_t enemySlashParticleCountSmash_ = 52;
    uint32_t enemySlashParticleCountSweep_ = 84;

    bool enemySwordTrailEnabled_ = true;
    float enemySwordTrailWidth_ = 1.0f;
};
