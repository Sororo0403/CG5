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

using namespace DirectX;

namespace {
PlayerCombatObservation MakePassivePlayerObservation(const Player &player) {
    PlayerCombatObservation playerObs{};
    playerObs.position = player.GetTransform().position;
    playerObs.velocity = player.GetVelocity();
    playerObs.isGuarding = false;
    playerObs.isCounterStance = false;
    playerObs.justCountered = false;
    playerObs.justCounterFailed = false;
    playerObs.justCounterEarly = false;
    playerObs.justCounterLate = false;
    playerObs.isAttacking = false;
    playerObs.counterAxis = CounterAxis::None;
    return playerObs;
}
} // namespace

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

    const DirectX::XMFLOAT3 &playerPos = player_.GetTransform().position;
    const DirectX::XMFLOAT3 &enemyPos = enemy_.GetTransform().position;
    battleCamera_.ResetForBattle(playerPos, enemyPos);
    introCamera_.Reset();
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
    gameFlow_.Reset();
    counterCinematicActive_ = false;
    if (ctx_ != nullptr && ctx_->renderer.postEffectRenderer != nullptr) {
        ctx_->renderer.postEffectRenderer->SetCounterVignetteActive(false);
        ctx_->renderer.postEffectRenderer->SetDemoPlayIndicatorVisible(false);
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

    const GameFlowController::UpdateResult flowResult =
        gameFlow_.Update(input, enemy_, enemyAnimation_, ctx_->model);
    if (flowResult.startedThisFrame) {
        counterCinematicActive_ = false;
        UpdateCounterVignette(baseDeltaTime);
        return;
    }

    if (flowResult.nonBattleFrame) {
        lighting_.Update(*ctx_, player_, enemy_, baseDeltaTime);

        ctx_->model->UpdateAnimation(playerModelId_, baseDeltaTime);

        PlayerCombatObservation playerObs =
            MakePassivePlayerObservation(player_);

        if (!freezeEnemyMotion) {
            enemy_.Update(playerObs, gameplayDeltaTime);
        }

        enemyAnimation_.Sync(ctx_->model, enemy_);
        enemyAnimation_.SetFrozen(ctx_->model, false);
        enemyAnimation_.Update(ctx_->model, gameplayDeltaTime);

        UpdateSceneCamera();
        counterCinematicActive_ = false;
        UpdateCounterVignette(baseDeltaTime);
        return;
    }

    ctx_->model->UpdateAnimation(playerModelId_, playerDeltaTime);

    player_.Update(input, playerDeltaTime, enemy_.GetTransform().position,
                   battleCamera_.GetYaw());

    const auto playerSlashStates = player_.GetSwordSlashStates();

    PlayerCombatObservation playerObs{};
    playerObs.position = player_.GetTransform().position;
    playerObs.velocity = player_.GetVelocity();
    playerObs.isGuarding = player_.IsGuarding();
    playerObs.isCounterStance = player_.IsCounterStance();
    playerObs.justCountered = player_.JustCountered();
    playerObs.justCounterFailed = player_.JustCounterFailed();
    playerObs.justCounterEarly = player_.JustCounterEarly();
    playerObs.justCounterLate = player_.JustCounterLate();
    playerObs.isAttacking = playerSlashStates[0] || playerSlashStates[1];

    switch (player_.GetCounterAxis()) {
    case SwordCounterAxis::Vertical:
        playerObs.counterAxis = CounterAxis::Vertical;
        break;

    case SwordCounterAxis::Horizontal:
        playerObs.counterAxis = CounterAxis::Horizontal;
        break;

    default:
        playerObs.counterAxis = CounterAxis::None;
        break;
    }

    if (!freezeEnemyMotion) {
        enemy_.Update(playerObs, enemyDeltaTime);
    }
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
    ctx_->renderer.postEffectRenderer->SetDemoPlayIndicatorVisible(
        gameFlow_.ShouldShowDemoIndicator());
    (void)deltaTime;
}

void GameScene::DrawCounterVignette() {
    if (ctx_ == nullptr || ctx_->renderer.postEffectRenderer == nullptr) {
        return;
    }
    ctx_->renderer.postEffectRenderer->DrawScreenOverlays();
}

void GameScene::DrawDemoPlayIndicator() {
    if (ctx_ == nullptr || ctx_->renderer.postEffectRenderer == nullptr) {
        return;
    }
    ctx_->renderer.postEffectRenderer->SetDemoPlayIndicatorVisible(
        gameFlow_.ShouldShowDemoIndicator());
}

void GameScene::Draw() {
    ctx_->model->PreDraw();
    const Camera *currentCamera = &camera_;
    if (currentCamera == nullptr) {
        ctx_->model->PostDraw();
        return;
    }

    player_.Draw(ctx_->model, *currentCamera);
    enemy_.Draw(ctx_->model, *currentCamera);
    ctx_->model->PostDraw();
    battleEffects_.Draw(*ctx_, enemy_, *currentCamera);
    DrawDemoPlayIndicator();
    DrawCounterVignette();
}

void GameScene::UpdateCamera(Input *input) { battleCamera_.UpdateInput(input); }

void GameScene::UpdateSceneCamera() {
    battleCamera_.Apply(camera_, ctx_->deltaTime, player_, enemy_);
    if (gameFlow_.IsIntro(enemy_)) {
        introCamera_.Apply(camera_, ctx_->deltaTime, enemy_);
    }
}

