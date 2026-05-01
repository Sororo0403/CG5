#pragma once
#include "Enemy.h"
#include <DirectXMath.h>
#include <cstdint>

class Camera;
struct SceneContext;

class BattleEffectDirector {
  public:
    void Reset();
    void BeginFrame(const SceneContext &ctx);
    void Update(const SceneContext &ctx, Enemy &enemy, const Camera &camera);
    void Draw(const SceneContext &ctx, const Enemy &enemy, const Camera &camera);

  private:
    bool ProjectWorldToScreen(const SceneContext &ctx, const Camera &camera,
                              const DirectX::XMFLOAT3 &worldPos,
                              DirectX::XMFLOAT2 &outScreen) const;
    bool ComputeEnemySlashWorldEffect(const Enemy &enemy,
                                      DirectX::XMFLOAT3 &outStart,
                                      DirectX::XMFLOAT3 &outEnd,
                                      float &outPhaseAlpha,
                                      float &outActionTime,
                                      ActionKind &outActionKind) const;
    void UpdateEnemySlashEffects(const SceneContext &ctx, const Enemy &enemy);
    void UpdateEnemySwordTrail(const SceneContext &ctx, const Enemy &enemy);
    void DrawWarpSmokePass(const SceneContext &ctx, const Enemy &enemy,
                           const Camera &camera);
    void DrawWarpDistortionPass(const SceneContext &ctx, const Enemy &enemy,
                                const Camera &camera);
    void SpawnElectricRing(const SceneContext &ctx,
                           const DirectX::XMFLOAT3 &worldPos,
                           bool isWarpEnd);
    void UpdateElectricRing(const SceneContext &ctx, const Camera &camera);

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

    float enemySlashChargePreviewAlpha_ = 0.22f;
    float enemySlashActiveAlpha_ = 0.92f;
    float enemySlashRecoveryAlpha_ = 0.36f;
    bool enemySlashActiveLatched_ = false;
    float enemySlashParticleEmitScale_ = 1.0f;
    uint32_t enemySlashParticleCountSmash_ = 52;
    uint32_t enemySlashParticleCountSweep_ = 84;

    bool enemySwordTrailEnabled_ = true;
    float enemySwordTrailWidth_ = 1.0f;
};
