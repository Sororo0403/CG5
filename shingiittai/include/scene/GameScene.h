#pragma once
#include "BattleCameraController.h"
#include "BaseScene.h"
#include "Bullet.h"
#include "Camera.h"
#include "Enemy.h"
#include "EffectParticleSystem.h"
#include "IEditableScene.h"
#include "IntroCameraController.h"
#include "Player.h"
#include "Transform.h"
#include <DirectXMath.h>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

class GameScene : public BaseScene, public IEditableScene {
  public:
    void Initialize(const SceneContext &ctx) override;
    void Update() override;
    void Draw() override;

    const Camera *GetCurrentCamera() const { return &camera_; }

    size_t GetEditableObjectCount() const override;
    IEditableObject *GetEditableObject(size_t index) override;
    const IEditableObject *GetEditableObject(size_t index) const override;
    uint64_t GetSelectedObjectId() const override;
    void SetSelectedObjectById(uint64_t id) override;
    int GetSelectedEditableObjectIndex() const override;
    void SetSelectedEditableObjectIndex(int index) override;
    void OnEditableObjectChanged(size_t index) override;
    bool SaveScene(std::string *message) override;
    bool LoadScene(std::string *message) override;
    bool NewScene(std::string *message) override;
    bool SaveSceneAs(const std::string &path, std::string *message) override;
    bool SaveSceneSnapshot(const std::string &path,
                           std::string *message) const override;
    bool LoadSceneFromPath(const std::string &path,
                           std::string *message) override;
    std::string GetCurrentScenePath() const override;
    std::string GetCurrentSceneName() const override;
    bool CaptureSceneState(std::string *outState,
                           std::string *message) const override;
    bool RestoreSceneState(const std::string &state,
                           std::string *message) override;
    bool IsSceneDirty() const override;
    void MarkSceneDirty() override;
    void ClearSceneDirty() override;
    bool CanEditObjects() const override;
    void SetGameplayPaused(bool paused) override;
    bool IsGameplayPaused() const override;
    void ResetGameplay() override;

  private:
    struct SceneEditableObject : public IEditableObject {
        GameScene *scene = nullptr;
        uint64_t id = 0;
        std::string name;
        std::string type;
        bool locked = false;
        bool visible = true;

        SceneEditableObject() = default;
        SceneEditableObject(GameScene *scene, uint64_t id, std::string name,
                            std::string type, bool locked, bool visible)
            : scene(scene), id(id), name(std::move(name)), type(std::move(type)),
              locked(locked), visible(visible) {}

        EditableObjectDesc GetEditorDesc() const override;
        void SetEditorName(const std::string &name) override;
        bool SetEditorLocked(bool locked) override;
        bool SetEditorVisible(bool visible) override;
        EditableTransform GetEditorTransform() const override;
        void SetEditorTransform(const EditableTransform &transform) override;
    };

    void UpdateCamera(Input *input);
    void UpdateSceneCamera();
    void UpdateSceneLighting();
    void SyncEnemyAnimation();
    void SetEnemyAnimationFrozen(bool frozen);
    float ComputeGameplayTimeScale() const;
    void UpdateCounterVignette(float deltaTime);
    void DrawCounterVignette();
    void DrawDemoPlayIndicator();
    bool ProjectWorldToScreen(const DirectX::XMFLOAT3 &worldPos,
                              DirectX::XMFLOAT2 &outScreen) const;
    bool ComputeEnemySlashScreenEffect(DirectX::XMFLOAT2 &outStart,
                                       DirectX::XMFLOAT2 &outEnd,
                                       float &outPhaseAlpha,
                                       float &outActionTime,
                                       ActionKind &outActionKind) const;
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
    static constexpr float kGuardDamageMultiplier = 0.25f;

    Camera camera_;
    BattleCameraController battleCamera_;
    IntroCameraController introCamera_;

    Player player_;
    Enemy enemy_;
    uint32_t playerModelId_ = 0;
    uint32_t enemyModelId_ = 0;
    std::string enemyAnimationName_{};
    bool enemyAnimationLoop_ = true;
    bool enemyIntroAnimationStarted_ = false;
    IntroPhase enemyIntroPhase_ = IntroPhase::SecondSlash;

    float playerHitCooldown_ = 0.0f;
    float enemyHitCooldown_ = 0.0f;

    bool dbgHitLeftHand_ = false;
    bool dbgHitRightHand_ = false;
    bool dbgHitBody_ = false;
    bool dbgBossHitPlayer_ = false;
    bool dbgBulletHitPlayer_ = false;
    bool dbgWaveHitPlayer_ = false;
    bool dbgPlayerGuardedHit_ = false;
    bool dbgTriggerCounterRequested_ = false;
    bool dbgTriggerWarpBackstabRequested_ = false;
#ifdef _DEBUG
    bool dbgFreezeEnemyMotion_ = false;
#endif
    Bullet bullet_;
    uint32_t warpSmokeSpriteId_ = 0;
    uint32_t warpSmokeDarkSpriteId_ = 0;

    float warpDistortionRadiusPx_ = 116.0f;
    float warpDistortionThicknessPx_ = 4.0f;
    float warpDistortionLineLengthPx_ = 72.0f;
    float warpDistortionAlpha_ = 0.34f;
    float warpDistortionMoveAlphaBonus_ = 0.20f;
    float warpDistortionJitterPx_ = 16.0f;
    float warpDistortionPreviewOffsetPx_ = 46.0f;
    float warpDistortionFootOffsetPx_ = 0.0f;
    float warpSmokeBaseSizePx_ = 132.0f;
    float warpSmokeMoveStretchPx_ = 92.0f;
    float warpSmokeAlpha_ = 0.34f;
    float warpSourceSmokeBloomScale_ = 1.42f;
    float warpSourceSmokeDriftPx_ = 30.0f;
    float warpSourceSmokeDarkAlpha_ = 0.58f;
    float warpSourceSmokeRedAlpha_ = 0.42f;
    float warpArrivalSmokeScale_ = 0.76f;
    float warpArrivalSmokeAlphaScale_ = 0.42f;
    float warpArrivalSmokeDenseScale_ = 1.58f;
    float warpArrivalSmokeDarkAlpha_ = 1.08f;
    float warpArrivalSmokeOffsetXPx_ = 54.0f;
    float warpArrivalSmokeOffsetYPx_ = 112.0f;
    float warpArrivalSmokeClusterRadiusPx_ = 74.0f;
    float warpMoveSmokeAlphaScale_ = 0.78f;
    float warpMoveSmokeStretchScale_ = 0.78f;
    float sceneLightTime_ = 0.0f;

    bool counterCinematicActive_ = false;
    bool hasGameStarted_ = false;
    bool demoIntroSkipped_ = false;
    bool enemyAnimationFrozen_ = false;
    float counterTimeScale_ = 0.05f;
    float demoPlayEffectTime_ = 0.0f;
    float counterCameraShakeX_ = 0.035f;
    float counterCameraShakeY_ = 0.020f;
    float counterCameraShakeFrequency_ = 18.0f;

    float reflectDamage_ = 0.0f;
    float damageMultiplier_ = 2.0f;

    float warpArrivalBurstBillboardSizePx_ = 18.0f;
    float warpArrivalBurstBillboardSpreadPx_ = 54.0f;
    float warpArrivalBurstBillboardAlpha_ = 0.52f;

    // Enemy slash presentation
    float enemySlashCoreThicknessPx_ = 8.0f;
    float enemySlashOuterThicknessPx_ = 18.0f;
    float enemySlashEchoOffsetPx_ = 14.0f;
    float enemySlashSparkSpreadPx_ = 70.0f;
    float enemySlashDistortionThicknessUv_ = 0.038f;
    float enemySlashDistortionStrength_ = 0.012f;
    float enemySlashChargePreviewAlpha_ = 0.22f;
    float enemySlashActiveAlpha_ = 0.92f;
    float enemySlashRecoveryAlpha_ = 0.36f;
    float warpArrivalFlashLengthPx_ = 172.0f;
    float warpArrivalFlashThicknessPx_ = 6.0f;
    float warpArrivalFlashGlowThicknessPx_ = 18.0f;

    // slash effect
    ActionKind prevEnemyActionKind_ = ActionKind::None;
    ActionStep prevEnemyActionStep_ = ActionStep::None;
    bool enemySlashActiveLatched_ = false;

    float enemySlashParticleEmitScale_ = 1.0f;
    uint32_t enemySlashParticleCountSmash_ = 52;
    uint32_t enemySlashParticleCountSweep_ = 84;

    bool enemySwordTrailEnabled_ = true;
    float enemySwordTrailWidth_ = 1.0f;

    std::vector<SceneEditableObject> editableObjects_{};
    uint64_t selectedEditableObjectId_ = 0;
    std::string currentScenePath_ = "resources/levels/shingiittai_editor.json";
    bool sceneDirty_ = false;
    bool gameplayPaused_ = false;
};
