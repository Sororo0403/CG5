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
#include "gameobject/EnemyAIComponent.h"
#include "gameobject/GameObject.h"
#include "gameobject/PlayerControllerComponent.h"
#include <DirectXMath.h>
#include <cstdint>
#include <memory>
#include <vector>

class GameScene : public BaseScene {
  public:
    void Initialize(const SceneContext &ctx) override;
    void Update() override;
    void Draw() override;

    const Camera *GetCurrentCamera() const { return &camera_; }

  private:
    void CreateGameObjects();
    void UpdateGameplayObjects(float playerDeltaTime, float enemyDeltaTime,
                               bool freezeEnemyMotion);
    void DrawGameplayObjects(const Camera &camera);
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
    std::vector<std::unique_ptr<GameObject>> gameObjects_;
    GameObject *playerObject_ = nullptr;
    GameObject *enemyObject_ = nullptr;
    PlayerControllerComponent *playerController_ = nullptr;
    EnemyAIComponent *enemyAI_ = nullptr;

#ifdef _DEBUG
    bool dbgFreezeEnemyMotion_ = false;
#endif

    bool counterCinematicActive_ = false;
    float counterTimeScale_ = 0.05f;
};
