#include "GameScene.h"
#include "DirectXCommon.h"
#include "EffectSystem.h"
#include "EngineRuntime.h"
#include "PlayerTuningPresetIO.h"
#include "Input.h"
#include "ModelManager.h"
#include "PostEffectRenderer.h"
#include "SceneManager.h"
#include "TextureManager.h"
#include "WinApp.h"
#include <string>
#ifndef IMGUI_DISABLED
#include "imgui.h"
#include "imgui_internal.h"
#endif
#include <algorithm>
#include <cmath>
#include "EnemyTuningPresetIO.h"

using namespace DirectX;

static const std::string kBossAnimIdle = "Action";
static const std::string kBossAnimMove = "Action.001";
static const std::string kBossAnimSweep =
    "\xE6\xA8\xAA\xE8\x96\x99\xE3\x81\x8E\xE6\x89\x95\xE3\x81\x84";
static const std::string kBossAnimWave =
    "\xE6\xB3\xA2\xE7\x8A\xB6\xE6\x94\xBB\xE6\x92\x83";
static const std::string kBossAnimSmash =
    "\xE7\xB8\xA6\xE6\x8C\xAF\xE3\x82\x8A\xE4\xB8\x8B\xE3\x82\x8D\xE3\x81\x97";
static bool HasAnimation(const Model *model, const std::string &animationName);

static std::string PickEnemyIntroAnimation(const Model *model, const Enemy &enemy,
                                           bool &outLoop) {
    outLoop = false;

    if (!model || model->animations.empty()) {
        return {};
    }

    switch (enemy.GetIntroPhase()) {
    case IntroPhase::SecondSlash:
        if (HasAnimation(model, kBossAnimSmash)) {
            return kBossAnimSmash;
        }
        if (HasAnimation(model, kBossAnimSweep)) {
            return kBossAnimSweep;
        }
        break;

    case IntroPhase::SpinSlash:
        if (HasAnimation(model, kBossAnimSweep)) {
            return kBossAnimSweep;
        }
        if (HasAnimation(model, kBossAnimSmash)) {
            return kBossAnimSmash;
        }
        break;

    case IntroPhase::Settle:
        if (HasAnimation(model, kBossAnimSweep)) {
            return kBossAnimSweep;
        }
        if (HasAnimation(model, kBossAnimSmash)) {
            return kBossAnimSmash;
        }
        break;
    }

    if (HasAnimation(model, kBossAnimIdle)) {
        outLoop = true;
        return kBossAnimIdle;
    }

    outLoop = true;
    return model->currentAnimation.empty() ? model->animations.begin()->first
                                           : model->currentAnimation;
}

static bool HasAnimation(const Model *model, const std::string &animationName) {
    if (!model) {
        return false;
    }

    return model->animations.find(animationName) != model->animations.end();
}

static std::string PickEnemyAnimation(const Model *model, const Enemy &enemy,
                                      bool &outLoop) {
    outLoop = true;

    if (!model || model->animations.empty()) {
        return {};
    }

    switch (enemy.GetActionKind()) {
    case ActionKind::Smash:
        outLoop = false;
        if (HasAnimation(model, kBossAnimSmash)) {
            return kBossAnimSmash;
        }
        break;

    case ActionKind::Sweep:
        outLoop = false;
        if (HasAnimation(model, kBossAnimSweep)) {
            return kBossAnimSweep;
        }
        break;

    case ActionKind::Shot:
    case ActionKind::Wave:
        outLoop = false;
        if (HasAnimation(model, kBossAnimWave)) {
            return kBossAnimWave;
        }
        break;

    case ActionKind::Warp:
    case ActionKind::Stalk:
    case ActionKind::None:
    default:
        if (HasAnimation(model, kBossAnimIdle)) {
            return kBossAnimIdle;
        }
        break;
    }

    if (HasAnimation(model, kBossAnimIdle)) {
        return kBossAnimIdle;
    }
    if (HasAnimation(model, kBossAnimMove)) {
        return kBossAnimMove;
    }

    return model->currentAnimation.empty() ? model->animations.begin()->first
                                           : model->currentAnimation;
}

static float Clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

static float EaseOutCubic(float t) {
    t = Clamp01(t);
    float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

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

    uint32_t playerModel = model->Load(upload, L"resources/model/player/player.glb");
    uint32_t swordModel = model->Load(upload, L"resources/model/player/sword.glb");
    uint32_t enemyModel = 0;
    uint32_t bulletModel =
        ctx_->model->Load(upload, L"resources/model/bullet/bullet.obj");
    try {
        enemyModel = model->Load(upload, L"resources/model/boss/boss.gltf");
    } catch (const std::exception &) {
        enemyModel = model->Load(upload, L"resources/model/enemy/enemy.glb");
    }

    upload.Finish();

    texture->ReleaseUploadBuffers();

    {
        PlayerTuningPreset playerPreset{};
        if (PlayerTuningPresetIO::Load("resources/player_tuning.txt",
                                       playerPreset)) {
            player_.ApplyTuningPreset(playerPreset);
        }
    }

    {
        EnemyTuningPreset enemyPreset{};
        if (EnemyTuningPresetIO::Load("resources/enemy_tuning.json",
                                      enemyPreset) ||
            EnemyTuningPresetIO::Load("resources/enemy_tuning.csv",
                                      enemyPreset) ||
            EnemyTuningPresetIO::Load("resources/enemy_tuning.txt",
                                      enemyPreset)) {
            enemy_.ApplyTuningPreset(enemyPreset);
        }
    }

    player_.Initialize(playerModel, swordModel);
    playerModelId_ = playerModel;
    enemy_.Initialize(enemyModel, bulletModel);
    enemyModelId_ = enemyModel;

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

    SyncEnemyAnimation();
    UpdateSceneLighting();
    combatSystem_.Reset();
    counterCinematicActive_ = false;
    hasGameStarted_ = false;
    demoIntroSkipped_ = false;
    enemyAnimationFrozen_ = false;
    if (ctx_ != nullptr && ctx_->renderer.postEffectRenderer != nullptr) {
        ctx_->renderer.postEffectRenderer->SetCounterVignetteActive(false);
        ctx_->renderer.postEffectRenderer->SetDemoPlayIndicatorVisible(false);
    }

    editableObjects_.clear();
    editableObjects_.push_back({this, 1, "Player", "Actor", false, true});
    editableObjects_.push_back({this, 2, "Enemy", "Actor", false, true});
    selectedEditableObjectId_ = editableObjects_.empty() ? 0 : editableObjects_[0].id;
    sceneDirty_ = false;
    gameplayPaused_ = false;
}

namespace {

EditableTransform ToEditableTransform(const Transform &transform) {
    return {transform.position, transform.rotation, transform.scale};
}

Transform FromEditableTransform(const EditableTransform &transform) {
    return {transform.position, transform.rotation, transform.scale};
}

} // namespace

EditableObjectDesc GameScene::SceneEditableObject::GetEditorDesc() const {
    EditableObjectDesc desc{};
    desc.id = id;
    desc.name = name;
    desc.type = type;
    desc.collider = "None";
    desc.editable = true;
    desc.locked = locked;
    desc.visible = visible;
    return desc;
}

void GameScene::SceneEditableObject::SetEditorName(const std::string &newName) {
    name = newName;
}

bool GameScene::SceneEditableObject::SetEditorLocked(bool newLocked) {
    locked = newLocked;
    return true;
}

bool GameScene::SceneEditableObject::SetEditorVisible(bool newVisible) {
    visible = newVisible;
    return true;
}

EditableTransform GameScene::SceneEditableObject::GetEditorTransform() const {
    if (scene == nullptr) {
        return {};
    }
    if (id == 1) {
        return ToEditableTransform(scene->player_.GetTransform());
    }
    if (id == 2) {
        return ToEditableTransform(scene->enemy_.GetTransform());
    }
    return {};
}

void GameScene::SceneEditableObject::SetEditorTransform(
    const EditableTransform &transform) {
    if (scene == nullptr) {
        return;
    }
    if (id == 1) {
        scene->player_.SetTransform(FromEditableTransform(transform));
    } else if (id == 2) {
        scene->enemy_.SetTransform(FromEditableTransform(transform));
    }
}

size_t GameScene::GetEditableObjectCount() const { return editableObjects_.size(); }

IEditableObject *GameScene::GetEditableObject(size_t index) {
    if (index >= editableObjects_.size()) {
        return nullptr;
    }
    return &editableObjects_[index];
}

const IEditableObject *GameScene::GetEditableObject(size_t index) const {
    if (index >= editableObjects_.size()) {
        return nullptr;
    }
    return &editableObjects_[index];
}

uint64_t GameScene::GetSelectedObjectId() const {
    return selectedEditableObjectId_;
}

void GameScene::SetSelectedObjectById(uint64_t id) {
    selectedEditableObjectId_ = 0;
    for (const SceneEditableObject &object : editableObjects_) {
        if (object.id == id) {
            selectedEditableObjectId_ = id;
            return;
        }
    }
}

int GameScene::GetSelectedEditableObjectIndex() const {
    for (size_t i = 0; i < editableObjects_.size(); ++i) {
        if (editableObjects_[i].id == selectedEditableObjectId_) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void GameScene::SetSelectedEditableObjectIndex(int index) {
    if (index < 0 || index >= static_cast<int>(editableObjects_.size())) {
        selectedEditableObjectId_ = 0;
        return;
    }
    selectedEditableObjectId_ = editableObjects_[static_cast<size_t>(index)].id;
}

void GameScene::OnEditableObjectChanged(size_t index) {
    if (index < editableObjects_.size()) {
        sceneDirty_ = true;
    }
}

bool GameScene::SaveScene(std::string *message) {
    if (message) {
        *message = "Scene save is not supported";
    }
    return false;
}

bool GameScene::LoadScene(std::string *message) {
    if (message) {
        *message = "Scene load is not supported";
    }
    return false;
}

bool GameScene::IsSceneDirty() const { return sceneDirty_; }

void GameScene::MarkSceneDirty() { sceneDirty_ = true; }

void GameScene::ClearSceneDirty() { sceneDirty_ = false; }

bool GameScene::CanEditObjects() const { return true; }

void GameScene::SetGameplayPaused(bool paused) { gameplayPaused_ = paused; }

bool GameScene::IsGameplayPaused() const { return gameplayPaused_; }

void GameScene::ResetGameplay() {
    player_.SetTransform({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f},
                          {1.0f, 1.0f, 1.0f}});
    combatSystem_.Reset();
}

void GameScene::Update() {
    Input *input = ctx_->input;
    const float baseDeltaTime = ctx_->deltaTime;
    const float gameplayDeltaTime = baseDeltaTime * ComputeGameplayTimeScale();
    const float playerDeltaTime =
        counterCinematicActive_ ? baseDeltaTime : gameplayDeltaTime;
    const float enemyDeltaTime =
        counterCinematicActive_ ? (baseDeltaTime * counterTimeScale_)
                               : gameplayDeltaTime;
#ifdef _DEBUG
    const bool freezeEnemyMotion = dbgFreezeEnemyMotion_;
#else
    const bool freezeEnemyMotion = false;
#endif

    UpdateCamera(input);

    if (!hasGameStarted_) {
        if (!demoIntroSkipped_) {
            enemy_.SkipIntro();
            demoIntroSkipped_ = true;
            SyncEnemyAnimation();
        }

        if (input != nullptr && input->IsKeyTrigger(DIK_SPACE)) {
            hasGameStarted_ = true;
            enemy_.RestartIntro();
            counterCinematicActive_ = false;
            SetEnemyAnimationFrozen(false);
            enemyAnimationName_.clear();
            enemyAnimationLoop_ = true;
            enemyIntroAnimationStarted_ = false;
            enemyIntroPhase_ = IntroPhase::SecondSlash;
            SyncEnemyAnimation();
            return;
        } else {
            sceneLightTime_ += baseDeltaTime;
            UpdateSceneLighting();

            ctx_->model->UpdateAnimation(playerModelId_, baseDeltaTime);

            PlayerCombatObservation playerObs{};
            playerObs.position = player_.GetTransform().position;
            playerObs.velocity = player_.GetVelocity();
            playerObs.isGuarding = false;
            playerObs.isCounterStance = false;
            playerObs.justCountered = false;
            playerObs.justCounterFailed = false;
            playerObs.justCounterEarly = false;
            playerObs.justCounterLate = false;
            playerObs.isAttacking = false;
            playerObs.counterAxis = CounterAxis::None;

            if (!freezeEnemyMotion) {
                enemy_.Update(playerObs, gameplayDeltaTime);
            }

            SyncEnemyAnimation();
            SetEnemyAnimationFrozen(false);
            if (!enemyAnimationFrozen_) {
                ctx_->model->UpdateAnimation(enemyModelId_, gameplayDeltaTime);
            }

            UpdateSceneCamera();
            counterCinematicActive_ = false;
            UpdateCounterVignette(baseDeltaTime);
            return;
        }
    }

    if (enemy_.IsIntroActive()) {
        sceneLightTime_ += baseDeltaTime;
        UpdateSceneLighting();

        ctx_->model->UpdateAnimation(playerModelId_, baseDeltaTime);

        PlayerCombatObservation playerObs{};
        playerObs.position = player_.GetTransform().position;
        playerObs.velocity = player_.GetVelocity();
        playerObs.isGuarding = false;
        playerObs.isCounterStance = false;
        playerObs.justCountered = false;
        playerObs.justCounterFailed = false;
        playerObs.justCounterEarly = false;
        playerObs.justCounterLate = false;
        playerObs.isAttacking = false;
        playerObs.counterAxis = CounterAxis::None;

        if (!freezeEnemyMotion) {
            enemy_.Update(playerObs, gameplayDeltaTime);
        }

        SyncEnemyAnimation();
        SetEnemyAnimationFrozen(false);
        if (!enemyAnimationFrozen_) {
            ctx_->model->UpdateAnimation(enemyModelId_, gameplayDeltaTime);
        }

        UpdateSceneCamera();
        counterCinematicActive_ = false;
        UpdateCounterVignette(baseDeltaTime);
        return;
    }

    ctx_->model->UpdateAnimation(playerModelId_, playerDeltaTime);
    // 蠖薙◁E��雁�E螳・
    // 蜈医↓繝励Ξ繧�E�繝､繝ｼ繧呈峩譁E��縺励※縲√◎縺�E�邨先棡繧脱nemy縺�E�貂｡縺・
    player_.Update(input, playerDeltaTime, enemy_.GetTransform().position,
                   battleCamera_.GetYaw());
    sceneLightTime_ += baseDeltaTime;

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
    UpdateSceneLighting();

    SyncEnemyAnimation();
    SetEnemyAnimationFrozen(counterCinematicActive_);
    if (!enemyAnimationFrozen_) {
        ctx_->model->UpdateAnimation(enemyModelId_, enemyDeltaTime);
    }

    if (ctx_ != nullptr && ctx_->effects != nullptr) {
        ctx_->effects->BeginFrame(ctx_->deltaTime);
    }

    UpdateEnemySlashEffects();

    UpdateEnemySwordTrail();

    enemy_.UpdatePresentationEvents();

    {
        auto requests = enemy_.ConsumeElectricRingSpawnRequests();
        for (const auto &req : requests) {
            SpawnElectricRing(req.worldPos, req.isWarpEnd);
        }
    }

    UpdateElectricRing();

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
            SetEnemyAnimationFrozen(true);
        }
        if (combatResult.stopCounterCinematicThisFrame) {
            counterCinematicActive_ = false;
            SetEnemyAnimationFrozen(false);
        }
        UpdateCounterVignette(baseDeltaTime);
        return;
    }

    if (combatResult.startCounterCinematicThisFrame) {
        counterCinematicActive_ = true;
        SetEnemyAnimationFrozen(true);
    }
    if (combatResult.stopCounterCinematicThisFrame) {
        counterCinematicActive_ = false;
        SetEnemyAnimationFrozen(false);
    }
    if (combatResult.forceSyncEnemyAnimationThisFrame) {
        SyncEnemyAnimation();
        SetEnemyAnimationFrozen(true);
    }

    UpdateCounterVignette(baseDeltaTime);
}

float GameScene::ComputeGameplayTimeScale() const {
    const EngineRuntime &runtime = EngineRuntime::GetInstance();
    if (gameplayPaused_) {
        return 0.0f;
    }
    if (runtime.IsEditorMode() && runtime.Settings().pauseGameInTuningMode) {
        return 0.0f;
    }
    return runtime.Settings().timeScale;
}

void GameScene::SetEnemyAnimationFrozen(bool frozen) {
    if (ctx_ == nullptr || ctx_->model == nullptr) {
        enemyAnimationFrozen_ = frozen;
        return;
    }

    Model *enemyModel = ctx_->model->GetModel(enemyModelId_);
    if (enemyModel == nullptr) {
        enemyAnimationFrozen_ = frozen;
        return;
    }

    if (frozen) {
        if (HasAnimation(enemyModel, kBossAnimIdle)) {
            ctx_->model->PlayAnimation(enemyModelId_, kBossAnimIdle, true);
            ctx_->model->UpdateAnimation(enemyModelId_, 0.0f);
            enemyAnimationName_ = kBossAnimIdle;
            enemyAnimationLoop_ = true;
        }
        enemyModel->isPlaying = false;
        enemyModel->animationTime = 0.0f;
        enemyModel->animationFinished = false;
        enemyAnimationFrozen_ = true;
        return;
    }

    if (enemyAnimationFrozen_ && !enemyModel->animationFinished &&
        !enemyModel->currentAnimation.empty()) {
        enemyModel->isPlaying = true;
    }
    enemyAnimationFrozen_ = false;
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
        !hasGameStarted_);
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
        !hasGameStarted_);
}

void GameScene::SyncEnemyAnimation() {
    ModelManager *modelManager = ctx_->model;
    Model *enemyModel = modelManager->GetModel(enemyModelId_);
    if (!enemyModel || enemyModel->animations.empty()) {
        return;
    }

    bool shouldLoop = true;
    std::string nextAnimation = PickEnemyAnimation(enemyModel, enemy_, shouldLoop);
    if (nextAnimation.empty()) {
        return;
    }

    if (enemy_.IsIntroActive()) {
        bool introLoop = false;
        std::string introAnimation =
            PickEnemyIntroAnimation(enemyModel, enemy_, introLoop);
        if (!introAnimation.empty()) {
            IntroPhase introPhase = enemy_.GetIntroPhase();
            const bool forceRestart =
                (enemyIntroPhase_ != introPhase &&
                 (introPhase == IntroPhase::SecondSlash ||
                  introPhase == IntroPhase::SpinSlash));
            if (!enemyIntroAnimationStarted_ || forceRestart ||
                enemyAnimationName_ != introAnimation ||
                enemyAnimationLoop_ != introLoop) {
                modelManager->PlayAnimation(enemyModelId_, introAnimation,
                                            introLoop);
                modelManager->UpdateAnimation(enemyModelId_, 0.0f);
                enemyAnimationName_ = introAnimation;
                enemyAnimationLoop_ = introLoop;
                enemyIntroAnimationStarted_ = true;
            }
            enemyIntroPhase_ = introPhase;
            return;
        }
    } else {
        enemyIntroAnimationStarted_ = false;
        enemyIntroPhase_ = IntroPhase::SecondSlash;
    }

    if (enemyAnimationName_ == nextAnimation && enemyAnimationLoop_ == shouldLoop) {
        return;
    }

    modelManager->PlayAnimation(enemyModelId_, nextAnimation, shouldLoop);
    modelManager->UpdateAnimation(enemyModelId_, 0.0f);
    enemyAnimationName_ = nextAnimation;
    enemyAnimationLoop_ = shouldLoop;
}
void GameScene::Draw() {
    ctx_->model->PreDraw();
    const Camera *currentCamera = &camera_;
    if (currentCamera == nullptr) {
        ctx_->model->PostDraw();
        return;
    }

    const bool playerVisible =
        editableObjects_.size() < 1 || editableObjects_[0].visible;
    const bool enemyVisible =
        editableObjects_.size() < 2 || editableObjects_[1].visible;
    if (playerVisible) {
        player_.Draw(ctx_->model, *currentCamera);
    }
    if (enemyVisible) {
        enemy_.Draw(ctx_->model, *currentCamera);
    }
    ctx_->model->PostDraw();
    if (EngineRuntime::GetInstance().IsEditorMode()) {
        return;
    }
    if (ctx_->effects != nullptr) {
        ctx_->effects->Draw(*currentCamera);
    }
    DrawWarpSmokePass();
    DrawWarpDistortionPass();
    DrawDemoPlayIndicator();
    DrawCounterVignette();

}

void GameScene::UpdateSceneLighting() {
    if (ctx_ == nullptr || ctx_->model == nullptr) {
        return;
    }

    const XMFLOAT3 &playerPos = player_.GetTransform().position;
    const XMFLOAT3 &enemyPos = enemy_.GetTransform().position;
    XMFLOAT3 accentAnchor = enemy_.GetTransform().position;
    if (enemy_.GetActionKind() == ActionKind::Warp) {
        accentAnchor = enemy_.GetWarpTargetPos();
    }

    const float pulse = 0.82f + 0.18f * std::sinf(sceneLightTime_ * 2.4f);
    const float actionBoost =
        enemy_.GetActionKind() == ActionKind::Warp ? 1.35f :
        enemy_.GetActionKind() == ActionKind::Wave ? 1.20f :
        enemy_.GetActionKind() == ActionKind::Shot ? 1.10f : 1.0f;

    XMFLOAT3 duelCenter = {
        (playerPos.x + enemyPos.x) * 0.5f,
        (playerPos.y + enemyPos.y) * 0.5f,
        (playerPos.z + enemyPos.z) * 0.5f,
    };

    SceneLighting lighting{};
    lighting.keyLightDirection = {-0.45f, -1.0f, 0.30f};
    lighting.keyLightColor = {1.25f, 1.14f, 1.02f, 1.0f};
    lighting.fillLightDirection = {0.70f, -0.28f, -0.65f};
    lighting.fillLightColor = {0.20f, 0.34f, 0.58f, 0.42f};
    lighting.ambientColor = {0.22f, 0.24f, 0.28f, 1.0f};
    lighting.lightingParams = {56.0f, 0.40f, 3.2f, 0.18f};

    lighting.pointLights[0].positionRange = {
        duelCenter.x,
        duelCenter.y + 2.4f,
        duelCenter.z - 0.7f,
        8.5f,
    };
    lighting.pointLights[0].colorIntensity = {
        1.00f,
        0.48f,
        0.26f,
        1.15f * pulse,
    };

    lighting.pointLights[1].positionRange = {
        accentAnchor.x,
        enemyPos.y + 1.6f,
        accentAnchor.z + 0.35f,
        6.8f,
    };
    lighting.pointLights[1].colorIntensity = {
        0.24f,
        0.52f,
        1.00f,
        0.70f * actionBoost,
    };

    lighting.pointLightCount = 2;
    ctx_->model->SetSceneLighting(lighting);
}

bool GameScene::ProjectWorldToScreen(const XMFLOAT3 &worldPos,
                                     XMFLOAT2 &outScreen) const {
    const Camera *currentCamera = &camera_;
    if (currentCamera == nullptr || ctx_ == nullptr || ctx_->winApp == nullptr) {
        return false;
    }

    XMMATRIX viewProj = currentCamera->GetView() * currentCamera->GetProj();
    XMVECTOR pos = XMVectorSet(worldPos.x, worldPos.y, worldPos.z, 1.0f);
    XMVECTOR clip = XMVector4Transform(pos, viewProj);
    float w = XMVectorGetW(clip);
    if (w <= 0.0001f) {
        return false;
    }

    float invW = 1.0f / w;
    float ndcX = XMVectorGetX(clip) * invW;
    float ndcY = XMVectorGetY(clip) * invW;
    float ndcZ = XMVectorGetZ(clip) * invW;
    if (ndcZ < 0.0f || ndcZ > 1.0f) {
        return false;
    }

    float width = static_cast<float>(ctx_->winApp->GetWidth());
    float height = static_cast<float>(ctx_->winApp->GetHeight());
    outScreen.x = (ndcX * 0.5f + 0.5f) * width;
    outScreen.y = (-ndcY * 0.5f + 0.5f) * height;
    return true;
}

bool GameScene::ComputeEnemySlashWorldEffect(XMFLOAT3 &outStart,
                                             XMFLOAT3 &outEnd,
                                             float &outPhaseAlpha,
                                             float &outActionTime,
                                             ActionKind &outActionKind) const {
    outStart = {};
    outEnd = {};
    outPhaseAlpha = 0.0f;
    outActionTime = 0.0f;
    outActionKind = ActionKind::None;

    const ActionKind actionKind = enemy_.GetActionKind();
    const ActionStep actionStep = enemy_.GetActionStep();
    const bool isEnemySlashAction =
        (actionKind == ActionKind::Smash || actionKind == ActionKind::Sweep) &&
        (actionStep == ActionStep::Charge || actionStep == ActionStep::Hold ||
         actionStep == ActionStep::Active ||
         actionStep == ActionStep::Recovery);

    if (!isEnemySlashAction) {
        return false;
    }

    XMFLOAT3 slashStartWorld = enemy_.GetRightHandTransform().position;
    XMFLOAT3 slashEndWorld = enemy_.GetAttackOBB().center;

    const float yaw =
        (actionStep == ActionStep::Charge || actionStep == ActionStep::Hold)
            ? enemy_.GetFacingYaw()
            : enemy_.GetLockedAttackYaw();

    const float forwardX = std::sinf(yaw);
    const float forwardZ = std::cosf(yaw);
    const float rightX = std::cosf(yaw);
    const float rightZ = -std::sinf(yaw);

    if (actionKind == ActionKind::Smash) {
        slashStartWorld.x += -forwardX * 0.35f;
        slashStartWorld.y += 0.95f;
        slashStartWorld.z += -forwardZ * 0.35f;

        slashEndWorld.x += forwardX * 0.55f;
        slashEndWorld.y -= 0.55f;
        slashEndWorld.z += forwardZ * 0.55f;
    } else {
        const XMFLOAT3 attackCenter = enemy_.GetAttackOBB().center;
        const float sweepHalfWidth = enemy_.GetSweepAttackBoxSize().x * 0.55f;

        slashStartWorld = {attackCenter.x - rightX * sweepHalfWidth,
                           attackCenter.y + 0.28f,
                           attackCenter.z - rightZ * sweepHalfWidth};

        slashEndWorld = {attackCenter.x + rightX * sweepHalfWidth,
                         attackCenter.y - 0.18f,
                         attackCenter.z + rightZ * sweepHalfWidth};
    }

    const AttackTimingParam *timing = enemy_.GetCurrentAttackTimingPublic();
    const float actionTime = enemy_.GetCurrentActionTimePublic();

    float phaseAlpha = 0.0f;
    if (actionStep == ActionStep::Charge || actionStep == ActionStep::Hold) {
        const float previewT =
            (timing != nullptr && timing->activeStartTime > 0.0001f)
                ? actionTime / timing->activeStartTime
                : 1.0f;
        phaseAlpha = enemySlashChargePreviewAlpha_ *
                     (0.35f + 0.65f * EaseOutCubic(previewT));
    } else if (actionStep == ActionStep::Active) {
        float activeT = 0.0f;
        if (timing != nullptr &&
            timing->activeEndTime > timing->activeStartTime) {
            activeT = (actionTime - timing->activeStartTime) /
                      (timing->activeEndTime - timing->activeStartTime);
        }
        phaseAlpha = enemySlashActiveAlpha_ * (1.0f - 0.22f * Clamp01(activeT));
    } else {
        float recoveryT = 0.0f;
        if (timing != nullptr &&
            timing->totalTime > timing->recoveryStartTime) {
            recoveryT = (actionTime - timing->recoveryStartTime) /
                        (timing->totalTime - timing->recoveryStartTime);
        }
        phaseAlpha =
            enemySlashRecoveryAlpha_ * (1.0f - EaseOutCubic(recoveryT));
    }

    if (phaseAlpha <= 0.01f) {
        return false;
    }

    outStart = slashStartWorld;
    outEnd = slashEndWorld;
    outPhaseAlpha = phaseAlpha;
    outActionTime = actionTime;
    outActionKind = actionKind;
    return true;
}

void GameScene::UpdateEnemySlashEffects() {
    const ActionKind actionKind = enemy_.GetActionKind();
    const ActionStep actionStep = enemy_.GetActionStep();

    DirectX::XMFLOAT3 slashStartWorld{};
    DirectX::XMFLOAT3 slashEndWorld{};
    float phaseAlpha = 0.0f;
    float actionTime = 0.0f;
    ActionKind worldActionKind = ActionKind::None;
    const bool hasSlashArc =
        ComputeEnemySlashWorldEffect(slashStartWorld, slashEndWorld, phaseAlpha,
                                     actionTime, worldActionKind);
    if (hasSlashArc && ctx_ != nullptr && ctx_->effects != nullptr) {
        EffectRequest request{};
        request.preset = EffectPreset::SlashArc;
        request.start = slashStartWorld;
        request.end = slashEndWorld;
        request.alpha = phaseAlpha;
        request.time = actionTime;
        request.duration = (std::max)(ctx_->deltaTime * 2.0f, 0.034f);
        request.sweep = (worldActionKind == ActionKind::Sweep);
        ctx_->effects->Play(request);
    }

    const bool isSlashActive =
        (actionKind == ActionKind::Smash || actionKind == ActionKind::Sweep) &&
        (actionStep == ActionStep::Active);

    if (isSlashActive && !enemySlashActiveLatched_) {
        if (hasSlashArc) {
            DirectX::XMFLOAT3 effectPosition{
                (slashStartWorld.x + slashEndWorld.x) * 0.5f,
                (slashStartWorld.y + slashEndWorld.y) * 0.5f,
                (slashStartWorld.z + slashEndWorld.z) * 0.5f};
            RequestEffect(EffectParticlePreset::ChargeSpark,
                          effectPosition);
            if (ctx_ != nullptr && ctx_->effects != nullptr) {
                const bool isSweep = (worldActionKind == ActionKind::Sweep);
                const uint32_t particleCount =
                    isSweep ? enemySlashParticleCountSweep_
                            : enemySlashParticleCountSmash_;
                ctx_->effects->EmitArcSparks(slashStartWorld, slashEndWorld,
                                             particleCount,
                                             enemySlashParticleEmitScale_,
                                             isSweep);
            }
        }
    }

    enemySlashActiveLatched_ = isSlashActive;
}

void GameScene::RequestEffect(EffectParticlePreset preset,
                              const DirectX::XMFLOAT3 &position) {
    if (ctx_ == nullptr || ctx_->effects == nullptr) {
        return;
    }

    ctx_->effects->EmitParticles(preset, position);
}

void GameScene::UpdateEnemySwordTrail() {
    if (ctx_ == nullptr || ctx_->effects == nullptr) {
        return;
    }

    ctx_->effects->SetTrailEnabled(enemySwordTrailEnabled_);

    const ActionKind actionKind = enemy_.GetActionKind();
    const ActionStep actionStep = enemy_.GetActionStep();

    const bool isTrailAction =
        (actionKind == ActionKind::Smash || actionKind == ActionKind::Sweep) &&
        (actionStep == ActionStep::Charge || actionStep == ActionStep::Active);

    if (isTrailAction) {
        DirectX::XMFLOAT3 baseWorld = enemy_.GetRightHandTransform().position;
        DirectX::XMFLOAT3 tipWorld = enemy_.GetAttackOBB().center;

        const float yaw = (actionStep == ActionStep::Charge)
                              ? enemy_.GetFacingYaw()
                              : enemy_.GetLockedAttackYaw();

        const float forwardX = std::sinf(yaw);
        const float forwardZ = std::cosf(yaw);
        const float rightX = std::cosf(yaw);
        const float rightZ = -std::sinf(yaw);

        if (actionKind == ActionKind::Smash) {
            baseWorld.x += -forwardX * 0.18f;
            baseWorld.y += 0.70f;
            baseWorld.z += -forwardZ * 0.18f;

            tipWorld.x += forwardX * 0.42f;
            tipWorld.y -= 0.38f;
            tipWorld.z += forwardZ * 0.42f;
        } else {
            const DirectX::XMFLOAT3 attackCenter = enemy_.GetAttackOBB().center;
            const float sweepHalfWidth =
                enemy_.GetSweepAttackBoxSize().x * 0.52f;

            baseWorld = {attackCenter.x - rightX * sweepHalfWidth,
                         attackCenter.y + 0.16f,
                         attackCenter.z - rightZ * sweepHalfWidth};

            tipWorld = {attackCenter.x + rightX * sweepHalfWidth,
                        attackCenter.y - 0.10f,
                        attackCenter.z + rightZ * sweepHalfWidth};
        }

        const float width = (actionKind == ActionKind::Sweep)
                                ? enemySwordTrailWidth_ * 1.10f
                                : enemySwordTrailWidth_ * 0.82f;

        ctx_->effects->EmitTrail(TrailRenderer::TrailPreset::SwordSlash,
                                 baseWorld, tipWorld, width);
    }
}

void GameScene::DrawWarpSmokePass() {
#ifdef IMGUI_DISABLED
    return;
#else
    if (enemy_.GetActionKind() != ActionKind::Warp) {
        return;
    }

    ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImDrawList *drawList = ImGui::GetBackgroundDrawList(viewport);
    if (viewport == nullptr || drawList == nullptr) {
        return;
    }

    ActionStep warpStep = enemy_.GetActionStep();
    float stepAlpha = 0.0f;
    switch (warpStep) {
    case ActionStep::Start:
        stepAlpha = 0.62f;
        break;
    case ActionStep::Move:
        stepAlpha = 0.78f;
        break;
    case ActionStep::End:
        stepAlpha = 0.92f;
        break;
    default:
        return;
    }

    XMFLOAT2 targetScreenF{};
    bool hasTarget = ProjectWorldToScreen(enemy_.GetWarpTargetPos(), targetScreenF);
    XMFLOAT2 sourceScreenF{};
    bool hasSource = enemy_.HasWarpDeparturePos() &&
                     ProjectWorldToScreen(enemy_.GetWarpDeparturePos(),
                                          sourceScreenF);

    auto drawSmoke = [&](const XMFLOAT2 &center, float scale, float alpha,
                         bool arrival) {
        const float time = enemy_.GetCurrentActionTimePublic();
        const int blobs = arrival ? 9 : 6;
        for (int i = 0; i < blobs; ++i) {
            const float r = static_cast<float>(i) / static_cast<float>(blobs);
            const float angle = time * (2.0f + r) + r * DirectX::XM_2PI * 1.7f;
            const float drift = (arrival ? 56.0f : 34.0f) * scale * (0.25f + r);
            const ImVec2 pos(center.x + std::cosf(angle) * drift,
                             center.y - 34.0f * scale +
                                 std::sinf(angle * 1.25f) * drift * 0.45f);
            const float radius =
                (arrival ? 72.0f : 48.0f) * scale * (0.65f + 0.55f * r);
            const int a = static_cast<int>(std::clamp(alpha * (1.0f - r * 0.18f),
                                                      0.0f, 1.0f) *
                                           115.0f);
            const ImU32 dark = IM_COL32(18, 6, 12, a);
            const ImU32 red = IM_COL32(180, 18, 38, static_cast<int>(a * 0.48f));
            drawList->AddCircleFilled(pos, radius, dark, 40);
            if ((i % 2) == 0) {
                drawList->AddCircle(pos, radius * 0.72f, red, 36, 2.0f);
            }
        }
    };

    if (hasSource && (warpStep == ActionStep::Start ||
                      warpStep == ActionStep::Move)) {
        drawSmoke(sourceScreenF, warpSourceSmokeBloomScale_, stepAlpha, false);
    }
    if (hasTarget) {
        XMFLOAT2 arrival = targetScreenF;
        arrival.y -= (warpStep == ActionStep::End) ? 18.0f : 46.0f;
        drawSmoke(arrival, warpArrivalSmokeDenseScale_, stepAlpha, true);
    }
#endif
}


void GameScene::DrawWarpDistortionPass() {
    if (ctx_ == nullptr || ctx_->renderer.postEffectRenderer == nullptr ||
        enemy_.GetActionKind() != ActionKind::Warp) {
        return;
    }

    ActionStep warpStep = enemy_.GetActionStep();
    float stepIntensity = 0.0f;
    DistortionPhase phase = DistortionPhase::Start;
    switch (warpStep) {
    case ActionStep::Start:
        stepIntensity = 0.72f;
        phase = DistortionPhase::Start;
        break;
    case ActionStep::Move:
        stepIntensity = 1.0f;
        phase = DistortionPhase::Sustain;
        break;
    case ActionStep::End:
        stepIntensity = 0.84f;
        phase = DistortionPhase::End;
        break;
    default:
        return;
    }

    XMFLOAT2 targetScreenF{};
    bool hasTarget = ProjectWorldToScreen(enemy_.GetWarpTargetPos(), targetScreenF);

    XMFLOAT2 sourceScreenF{};
    bool hasSource = enemy_.HasWarpDeparturePos() &&
                     ProjectWorldToScreen(enemy_.GetWarpDeparturePos(),
                                          sourceScreenF);
    if (!hasSource && !hasTarget) {
        return;
    }

    float time = enemy_.GetCurrentActionTimePublic();
    float baseRadius = warpDistortionRadiusPx_ * (0.85f + 0.30f * stepIntensity);
    float jitter = warpDistortionJitterPx_ * (0.80f + 0.40f * std::sinf(time * 20.0f));
    float alpha = warpDistortionAlpha_ + warpDistortionMoveAlphaBonus_ *
                                              (warpStep == ActionStep::Move ? 1.0f : 0.0f);

    DistortionEffectParams params{};
    params.preset = DistortionPreset::WarpPortal;
    params.hasOrigin = hasSource;
    params.hasTarget = hasTarget;
    params.phase = phase;
    params.originScreen = sourceScreenF;
    params.targetScreen = targetScreenF;
    params.time = time;
    params.intensity = stepIntensity;
    params.baseRadius = baseRadius;
    params.jitter = jitter;
    params.alpha = alpha;
    params.thickness = warpDistortionThicknessPx_;
    params.lineLength = warpDistortionLineLengthPx_;
    params.footOffset = warpDistortionFootOffsetPx_;
    params.previewOffset = warpDistortionPreviewOffsetPx_;
    ctx_->renderer.postEffectRenderer->Request(
        PostEffectRenderer::PostEffectType::Distortion, params);
}

void GameScene::UpdateCamera(Input *input) {
    battleCamera_.UpdateInput(input);
}

void GameScene::UpdateSceneCamera() {
    battleCamera_.Apply(camera_, ctx_->deltaTime, player_, enemy_);
    if (enemy_.IsIntroActive()) {
        introCamera_.Apply(camera_, ctx_->deltaTime, enemy_);
    }
}

////////////////////////////
//lightring
///////////////////////////
void GameScene::SpawnElectricRing(const XMFLOAT3 &worldPos, bool isWarpEnd) {
    if (ctx_ == nullptr || ctx_->renderer.postEffectRenderer == nullptr) {
        return;
    }
    ctx_->renderer.postEffectRenderer->Request(
        isWarpEnd ? PostEffectRenderer::PostEffectType::WarpRingEnd
                  : PostEffectRenderer::PostEffectType::WarpRingStart,
        worldPos);
}

void GameScene::UpdateElectricRing() {
    const Camera *currentCamera = &camera_;
    if (ctx_ == nullptr || ctx_->renderer.postEffectRenderer == nullptr ||
        currentCamera == nullptr) {
        return;
    }
    ctx_->renderer.postEffectRenderer->UpdateScreenEffects(
        ctx_->deltaTime, *currentCamera, ctx_->frame.width, ctx_->frame.height);
}

