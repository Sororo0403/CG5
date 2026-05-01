#pragma once
#include "BattleCameraController.h"
#include "BaseScene.h"
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
    bool enemyAnimationFrozen_ = false;
    float counterTimeScale_ = 0.05f;

    float reflectDamage_ = 0.0f;
    float damageMultiplier_ = 2.0f;

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

    std::vector<SceneEditableObject> editableObjects_{};
    uint64_t selectedEditableObjectId_ = 0;
    std::string currentScenePath_ = "resources/levels/shingiittai_editor.json";
    bool sceneDirty_ = false;
    bool gameplayPaused_ = false;
};
