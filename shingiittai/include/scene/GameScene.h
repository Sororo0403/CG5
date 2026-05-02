#pragma once
#include "BattleCameraController.h"
#include "BattleEffectDirector.h"
#include "BaseScene.h"
#include "Camera.h"
#include "CollisionSystem.h"
#include "CombatSystem.h"
#include "EnemyAISystem.h"
#include "Enemy.h"
#include "EnemyAnimationController.h"
#include "GameRuleSystem.h"
#include "InputSystem.h"
#include "LightingController.h"
#include "MovementSystem.h"
#include "PhysicsSystem.h"
#include "Player.h"
#include "PlayerControlSystem.h"
#include "RenderSystem.h"
#include "World.h"
#include "gameobject/EnemyAIComponent.h"
#include "gameobject/GameObject.h"
#include "gameobject/PlayerControllerComponent.h"
#include <DirectXMath.h>
#include <cstdint>
#include <memory>
#include <vector>

class GameScene : public BaseScene {
  public:
    /// <summary>
    /// シーン初期化を行い、ゲームオブジェクトとECS Entityを生成する。
    /// </summary>
    /// <param name="ctx">EngineのSceneContext。</param>
    void Initialize(const SceneContext &ctx) override;

    /// <summary>
    /// シーン更新を行い、既存ロジックとECS Systemを実行する。
    /// </summary>
    void Update() override;

    /// <summary>
    /// シーン描画を行う。
    /// </summary>
    void Draw() override;

    /// <summary>
    /// 現在の描画Cameraを取得する。
    /// </summary>
    /// <returns>現在のCamera。</returns>
    const Camera *GetCurrentCamera() const { return &camera_; }

  private:
    /// <summary>
    /// 既存GameObjectを生成する。
    /// </summary>
    void CreateGameObjects();

    /// <summary>
    /// ECS Worldにプレイヤーと敵Entityを生成する。
    /// </summary>
    void CreateEcsEntities();

    /// <summary>
    /// ECS Systemを実行する。
    /// </summary>
    /// <param name="deltaTime">前フレームからの経過時間。</param>
    void UpdateEcs(float deltaTime);

    /// <summary>
    /// 既存GameObjectベースのゲームプレイ更新を行う。
    /// </summary>
    /// <param name="playerDeltaTime">プレイヤー更新用の経過時間。</param>
    /// <param name="enemyDeltaTime">敵更新用の経過時間。</param>
    /// <param name="freezeEnemyMotion">敵の移動を停止するか。</param>
    void UpdateGameplayObjects(float playerDeltaTime, float enemyDeltaTime,
                               bool freezeEnemyMotion);

    /// <summary>
    /// 既存GameObjectベースの描画を行う。
    /// </summary>
    /// <param name="camera">描画に使用するCamera。</param>
    void DrawGameplayObjects(const Camera &camera);

    /// <summary>
    /// バトルカメラ入力を更新する。
    /// </summary>
    /// <param name="input">入力管理オブジェクト。</param>
    void UpdateCamera(Input *input);

    /// <summary>
    /// シーンCameraへバトルカメラ状態を反映する。
    /// </summary>
    void UpdateSceneCamera();

    /// <summary>
    /// カウンター演出のビネット状態を更新する。
    /// </summary>
    /// <param name="deltaTime">前フレームからの経過時間。</param>
    void UpdateCounterVignette(float deltaTime);

    /// <summary>
    /// カウンター演出のビネットを描画する。
    /// </summary>
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

    World ecsWorld_;
    Entity ecsPlayerEntity_ = kInvalidEntity;
    Entity ecsEnemyEntity_ = kInvalidEntity;
    InputSystem ecsInputSystem_;
    PlayerControlSystem ecsPlayerControlSystem_;
    EnemyAISystem ecsEnemyAISystem_;
    PhysicsSystem ecsPhysicsSystem_;
    MovementSystem ecsMovementSystem_;
    CollisionSystem ecsCollisionSystem_;
    GameRuleSystem ecsGameRuleSystem_;
    RenderSystem ecsRenderSystem_;

#ifdef _DEBUG
    bool dbgFreezeEnemyMotion_ = false;
#endif

    bool counterCinematicActive_ = false;
    float counterTimeScale_ = 0.05f;
};
