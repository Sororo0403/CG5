#include "GameScene.h"
#include "DirectXCommon.h"
#include "EnemyTuningPresetIO.h"
#include "Input.h"
#include "ModelManager.h"
#include "PlayerTuningPresetIO.h"
#include "PostEffectRenderer.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "WinApp.h"
#include "gameobject/ColliderComponent.h"
#include "gameobject/RenderComponent.h"

#include <memory>
#include <utility>

using namespace DirectX;

void GameScene::Initialize(const SceneContext &ctx) {
    BaseScene::Initialize(ctx);

    float aspect = static_cast<float>(ctx_->winApp->GetWidth()) /
                   static_cast<float>(ctx_->winApp->GetHeight());

    camera_.Initialize(aspect);
    camera_.SetMode(CameraMode::LookAt);
    camera_.SetPerspectiveFovDeg(74.0f);
    camera_.UpdateMatrices();

    DirectXCommon *dx = ctx_->dxCommon;
    ModelManager *model = ctx_->model;
    TextureManager *texture = ctx_->texture;

    auto upload = dx->BeginUploadContext();

    uint32_t playerModel =
        model->Load(upload, L"resources/model/player/player.glb");
    uint32_t swordModel =
        model->Load(upload, L"resources/model/player/sword.glb");
    uint32_t enemyModel = 0;
    uint32_t bulletModel =
        ctx_->model->Load(upload, L"resources/model/bullet/bullet.obj");
    enemyModel = model->Load(upload, L"resources/model/boss/boss.glb");

    upload.Finish();

    texture->ReleaseUploadBuffers();

    player_.Initialize(playerModel, swordModel);
    playerModelId_ = playerModel;
    enemy_.Initialize(enemyModel, bulletModel);
    enemyModelId_ = enemyModel;
    enemy_.ApplyAnimationTimingsFromModel(model->GetModel(enemyModelId_));
    enemyAnimation_.Initialize(enemyModelId_);
    CreateGameObjects();

    const DirectX::XMFLOAT3 &playerPos = player_.GetTransform().position;
    const DirectX::XMFLOAT3 &enemyPos = enemy_.GetTransform().position;
    battleCamera_.ResetForBattle(playerPos, enemyPos);
    if (Model *playerModelData = model->GetModel(playerModelId_)) {
        if (!playerModelData->animations.empty()) {
            model->PlayAnimation(playerModelId_,
                                 playerModelData->currentAnimation, true);
        }
    }

    enemyAnimation_.Sync(ctx_->model, enemy_);
    lighting_.Reset();
    lighting_.Update(*ctx_, player_, enemy_, 0.0f);
    combatSystem_.Reset();
    battleEffects_.Reset();
    counterCinematicActive_ = false;
    if (ctx_ != nullptr && ctx_->renderer.postEffectRenderer != nullptr) {
        ctx_->renderer.postEffectRenderer->SetCounterVignetteActive(false);
    }
}

void GameScene::CreateGameObjects() {
    gameObjects_.clear();
    playerObject_ = nullptr;
    enemyObject_ = nullptr;
    playerController_ = nullptr;
    enemyAI_ = nullptr;

    auto playerObject = std::make_unique<GameObject>("Player");
    playerObject->GetTransformComponent().SetTransform(player_.GetTransform());
    playerController_ =
        &playerObject->AddComponent<PlayerControllerComponent>(&player_,
                                                               ctx_->input);
    playerController_->SetLookTargetProvider(
        [this]() { return enemy_.GetTransform().position; });
    playerController_->SetCameraYawProvider(
        [this]() { return battleCamera_.GetYaw(); });
    playerObject->AddComponent<ColliderComponent>(
        ColliderLayer::Player, [this]() { return player_.GetOBB(); });
    playerObject->AddComponent<RenderComponent>(
        ctx_->model, playerModelId_,
        [this](ModelManager *modelManager, const Camera &camera) {
            player_.Draw(modelManager, camera);
        });
    playerObject_ = playerObject.get();
    gameObjects_.push_back(std::move(playerObject));

    auto enemyObject = std::make_unique<GameObject>("Enemy");
    enemyObject->GetTransformComponent().SetTransform(enemy_.GetTransform());
    enemyAI_ = &enemyObject->AddComponent<EnemyAIComponent>(&enemy_);
    enemyAI_->SetObservationProvider([this]() {
        if (playerController_ == nullptr) {
            return PlayerCombatObservation{};
        }
        return playerController_->BuildCombatObservation();
    });
    enemyObject->AddComponent<ColliderComponent>(
        ColliderLayer::Enemy, [this]() { return enemy_.GetBodyOBB(); });
    enemyObject->AddComponent<RenderComponent>(
        ctx_->model, enemyModelId_,
        [this](ModelManager *modelManager, const Camera &camera) {
            enemy_.Draw(modelManager, camera);
        });
    enemyObject_ = enemyObject.get();
    gameObjects_.push_back(std::move(enemyObject));
}

void GameScene::UpdateGameplayObjects(float playerDeltaTime,
                                      float enemyDeltaTime,
                                      bool freezeEnemyMotion) {
    if (playerController_ != nullptr) {
        playerController_->SetInput(ctx_->input);
    }
    if (playerObject_ != nullptr) {
        playerObject_->Update(playerDeltaTime);
    }

    if (enemyAI_ != nullptr) {
        enemyAI_->SetFrozen(freezeEnemyMotion);
    }
    if (enemyObject_ != nullptr) {
        enemyObject_->Update(enemyDeltaTime);
    }
}

void GameScene::DrawGameplayObjects(const Camera &camera) {
    for (auto &object : gameObjects_) {
        if (object != nullptr) {
            object->Draw(camera);
        }
    }
}

void GameScene::Update() {
    Input *input = ctx_->input;
    const float baseDeltaTime = ctx_->deltaTime;
    const float gameplayDeltaTime = baseDeltaTime;
    const float playerDeltaTime =
        counterCinematicActive_ ? baseDeltaTime : gameplayDeltaTime;
    const float enemyDeltaTime = counterCinematicActive_
                                     ? (baseDeltaTime * counterTimeScale_)
                                     : gameplayDeltaTime;
#ifdef _DEBUG
    const bool freezeEnemyMotion = dbgFreezeEnemyMotion_;
#else
    const bool freezeEnemyMotion = false;
#endif

    UpdateCamera(input);

    ctx_->model->UpdateAnimation(playerModelId_, playerDeltaTime);

    UpdateGameplayObjects(playerDeltaTime, enemyDeltaTime, freezeEnemyMotion);
    lighting_.Update(*ctx_, player_, enemy_, baseDeltaTime);

    enemyAnimation_.Sync(ctx_->model, enemy_);
    enemyAnimation_.SetFrozen(ctx_->model, counterCinematicActive_);
    enemyAnimation_.Update(ctx_->model, enemyDeltaTime);

    battleEffects_.BeginFrame(*ctx_);
    battleEffects_.Update(*ctx_, enemy_, camera_);

    UpdateSceneCamera();

    CombatSystem::ResolveContext combatContext{};
    combatContext.deltaTime = gameplayDeltaTime;
    combatContext.freezeEnemyMotion = freezeEnemyMotion;
    combatContext.counterCinematicActive = counterCinematicActive_;
    const auto combatResult =
        combatSystem_.Resolve(player_, enemy_, ctx_, combatContext);

    if (freezeEnemyMotion) {
        if (combatResult.startCounterCinematicThisFrame) {
            counterCinematicActive_ = true;
            enemyAnimation_.SetFrozen(ctx_->model, true);
        }
        if (combatResult.stopCounterCinematicThisFrame) {
            counterCinematicActive_ = false;
            enemyAnimation_.SetFrozen(ctx_->model, false);
        }
        UpdateCounterVignette(baseDeltaTime);
        return;
    }

    if (combatResult.startCounterCinematicThisFrame) {
        counterCinematicActive_ = true;
        enemyAnimation_.SetFrozen(ctx_->model, true);
    }
    if (combatResult.stopCounterCinematicThisFrame) {
        counterCinematicActive_ = false;
        enemyAnimation_.SetFrozen(ctx_->model, false);
    }
    if (combatResult.forceSyncEnemyAnimationThisFrame) {
        enemyAnimation_.Sync(ctx_->model, enemy_);
        enemyAnimation_.SetFrozen(ctx_->model, true);
    }

    UpdateCounterVignette(baseDeltaTime);
}

void GameScene::UpdateCounterVignette(float deltaTime) {
    const Camera *currentCamera = &camera_;
    if (ctx_ == nullptr || ctx_->renderer.postEffectRenderer == nullptr ||
        currentCamera == nullptr) {
        return;
    }
    ctx_->renderer.postEffectRenderer->SetCounterVignetteActive(
        counterCinematicActive_);
    (void)deltaTime;
}

void GameScene::DrawCounterVignette() {
    if (ctx_ == nullptr || ctx_->renderer.postEffectRenderer == nullptr) {
        return;
    }
    ctx_->renderer.postEffectRenderer->DrawScreenOverlays();
}

void GameScene::Draw() {
    ctx_->model->PreDraw();
    const Camera *currentCamera = &camera_;
    if (currentCamera == nullptr) {
        ctx_->model->PostDraw();
        return;
    }

    DrawGameplayObjects(*currentCamera);
    ctx_->model->PostDraw();
    battleEffects_.Draw(*ctx_, enemy_, *currentCamera);
    DrawCounterVignette();
}

void GameScene::UpdateCamera(Input *input) { battleCamera_.UpdateInput(input); }

void GameScene::UpdateSceneCamera() {
    battleCamera_.Apply(camera_, ctx_->deltaTime, player_, enemy_);
}

