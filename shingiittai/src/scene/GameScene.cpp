#include "GameScene.h"
#include "CollisionUtil.h"
#include "DirectXCommon.h"
#include "EffectSystem.h"
#include "EngineRuntime.h"
#include "PlayerTuningPresetIO.h"
#include "Input.h"
#include "ModelManager.h"
#include "PostEffectRenderer.h"
#include "SceneManager.h"
#include "SpriteManager.h"
#include "TextureManager.h"
#include "WinApp.h"
#include <string>
#ifdef _DEBUG
#include "DebugDraw.h"
#endif // _DEBUG
#ifndef IMGUI_DISABLED
#include "imgui.h"
#include "imgui_internal.h"
#endif
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
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
    warpSmokeSpriteId_ =
        ctx_->sprite->Create(upload, L"resources/texture/effect/warp_smoke.png");
    warpSmokeDarkSpriteId_ =
        ctx_->sprite->Create(upload, L"resources/texture/effect/warp_smoke_dark.png");
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
    bullet_.Initialize(bulletModel);

    SyncEnemyAnimation();
    UpdateSceneLighting();
    counterCinematicActive_ = false;
    hasGameStarted_ = false;
    demoIntroSkipped_ = false;
    enemyAnimationFrozen_ = false;
    demoPlayEffectTime_ = 0.0f;
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

nlohmann::json ToJson(const Transform &transform) {
    return {
        {"position",
         {transform.position.x, transform.position.y, transform.position.z}},
        {"rotation",
         {transform.rotation.x, transform.rotation.y, transform.rotation.z,
          transform.rotation.w}},
        {"scale", {transform.scale.x, transform.scale.y, transform.scale.z}},
    };
}

Transform TransformFromJson(const nlohmann::json &json,
                            const Transform &fallback) {
    Transform transform = fallback;
    if (json.contains("position") && json["position"].is_array() &&
        json["position"].size() >= 3) {
        transform.position = {json["position"][0].get<float>(),
                              json["position"][1].get<float>(),
                              json["position"][2].get<float>()};
    }
    if (json.contains("rotation") && json["rotation"].is_array() &&
        json["rotation"].size() >= 4) {
        transform.rotation = {json["rotation"][0].get<float>(),
                              json["rotation"][1].get<float>(),
                              json["rotation"][2].get<float>(),
                              json["rotation"][3].get<float>()};
    }
    if (json.contains("scale") && json["scale"].is_array() &&
        json["scale"].size() >= 3) {
        transform.scale = {json["scale"][0].get<float>(),
                           json["scale"][1].get<float>(),
                           json["scale"][2].get<float>()};
    }
    return transform;
}

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
    return SaveSceneAs(currentScenePath_, message);
}

bool GameScene::LoadScene(std::string *message) {
    return LoadSceneFromPath(currentScenePath_, message);
}

bool GameScene::NewScene(std::string *message) {
    player_.SetTransform({{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f},
                          {1.0f, 1.0f, 1.0f}});
    enemy_.SetTransform({{0.0f, 0.0f, 6.0f}, {0.0f, 0.0f, 0.0f, 1.0f},
                         {1.0f, 1.0f, 1.0f}});
    currentScenePath_.clear();
    sceneDirty_ = true;
    if (message) {
        *message = "New Shingiittai editor scene created";
    }
    return true;
}

bool GameScene::SaveSceneAs(const std::string &path, std::string *message) {
    const bool saved = SaveSceneSnapshot(path, message);
    if (saved) {
        currentScenePath_ = path;
        sceneDirty_ = false;
    }
    return saved;
}

bool GameScene::SaveSceneSnapshot(const std::string &path,
                                  std::string *message) const {
    try {
        std::filesystem::path scenePath(path);
        if (scenePath.has_parent_path()) {
            std::filesystem::create_directories(scenePath.parent_path());
        }

        nlohmann::json json;
        json["name"] = GetCurrentSceneName();
        json["objects"]["player"] = ToJson(player_.GetTransform());
        json["objects"]["enemy"] = ToJson(enemy_.GetTransform());

        std::ofstream ofs(scenePath);
        ofs << json.dump(2);
        if (message) {
            *message = "Saved scene: " + scenePath.string();
        }
        return true;
    } catch (const std::exception &e) {
        if (message) {
            *message = e.what();
        }
        return false;
    }
}

bool GameScene::LoadSceneFromPath(const std::string &path,
                                  std::string *message) {
    try {
        std::ifstream ifs(path);
        if (!ifs) {
            if (message) {
                *message = "Scene file not found: " + path;
            }
            return false;
        }
        nlohmann::json json;
        ifs >> json;
        const auto &objects = json["objects"];
        if (objects.contains("player")) {
            player_.SetTransform(
                TransformFromJson(objects["player"], player_.GetTransform()));
        }
        if (objects.contains("enemy")) {
            enemy_.SetTransform(
                TransformFromJson(objects["enemy"], enemy_.GetTransform()));
        }
        currentScenePath_ = path;
        sceneDirty_ = false;
        if (message) {
            *message = "Loaded scene: " + path;
        }
        return true;
    } catch (const std::exception &e) {
        if (message) {
            *message = e.what();
        }
        return false;
    }
}

std::string GameScene::GetCurrentScenePath() const { return currentScenePath_; }

std::string GameScene::GetCurrentSceneName() const {
    if (currentScenePath_.empty()) {
        return "Untitled";
    }
    return std::filesystem::path(currentScenePath_).stem().string();
}

bool GameScene::CaptureSceneState(std::string *outState,
                                  std::string *message) const {
    if (outState == nullptr) {
        if (message) {
            *message = "Missing output state";
        }
        return false;
    }

    nlohmann::json json;
    json["objects"]["player"] = ToJson(player_.GetTransform());
    json["objects"]["enemy"] = ToJson(enemy_.GetTransform());
    json["selected"] = selectedEditableObjectId_;
    json["dirty"] = sceneDirty_;
    *outState = json.dump();
    if (message) {
        *message = "Scene state captured";
    }
    return true;
}

bool GameScene::RestoreSceneState(const std::string &state,
                                  std::string *message) {
    try {
        nlohmann::json json = nlohmann::json::parse(state);
        const auto &objects = json["objects"];
        if (objects.contains("player")) {
            player_.SetTransform(
                TransformFromJson(objects["player"], player_.GetTransform()));
        }
        if (objects.contains("enemy")) {
            enemy_.SetTransform(
                TransformFromJson(objects["enemy"], enemy_.GetTransform()));
        }
        selectedEditableObjectId_ = json.value("selected", selectedEditableObjectId_);
        sceneDirty_ = json.value("dirty", sceneDirty_);
        if (message) {
            *message = "Scene state restored";
        }
        return true;
    } catch (const std::exception &e) {
        if (message) {
            *message = e.what();
        }
        return false;
    }
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

    if (input != nullptr && input->IsKeyTrigger(DIK_F1)) {
        dbgTriggerCounterRequested_ = true;
    }
    if (input != nullptr && input->IsKeyTrigger(DIK_F2)) {
        dbgTriggerWarpBackstabRequested_ = true;
    }
    UpdateCamera(input);

    if (!hasGameStarted_) {
        demoPlayEffectTime_ += baseDeltaTime;
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

    if (dbgTriggerWarpBackstabRequested_ && !freezeEnemyMotion) {
        dbgTriggerWarpBackstabRequested_ = false;
        enemy_.DebugTriggerWarpBackstab(playerObs);
        SyncEnemyAnimation();
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

    auto counterBox = player_.GetSword().GetCounterOBB();
    auto playerBox = player_.GetOBB();
    const bool isPlayerGuarding = player_.IsGuarding();
    const bool isPlayerCountering = player_.GetSword().IsSlashMode();
    const auto enemyBodyBox = enemy_.GetBodyOBB();
    const ActionKind enemyActionKind = enemy_.GetActionKind();
    const ActionStep enemyActionStep = enemy_.GetActionStep();
    bool startCounterCinematicThisFrame = false;
    bool stopCounterCinematicThisFrame = false;
    bool forceSyncEnemyAnimationThisFrame = false;
    auto triggerSuccessfulCounter = [&](float enemyDamage, float hitCooldown) {
        player_.NotifyCounterSuccess();
        if (enemy_.NotifyCountered()) {
            forceSyncEnemyAnimationThisFrame = true;
        }
        enemy_.TakeDamage(enemyDamage);
        RequestEffect(EffectParticlePreset::HitSpark,
                      enemy_.GetTransform().position);
        playerHitCooldown_ = hitCooldown;
        startCounterCinematicThisFrame = true;
    };

    if (dbgTriggerCounterRequested_) {
        dbgTriggerCounterRequested_ = false;
        triggerSuccessfulCounter(
            (std::max)(enemy_.GetCurrentAttackDamage(), 1.0f), 0.2f);
    }

    if (enemyHitCooldown_ > 0.0f) {
        enemyHitCooldown_ -= gameplayDeltaTime;
        if (enemyHitCooldown_ < 0.0f) {
            enemyHitCooldown_ = 0.0f;
        }
    }

    if (playerHitCooldown_ > 0.0f) {
        playerHitCooldown_ -= gameplayDeltaTime;
        if (playerHitCooldown_ < 0.0f) {
            playerHitCooldown_ = 0.0f;
        }
    }

    dbgHitLeftHand_ = false;
    dbgHitRightHand_ = false;
    dbgHitBody_ = false;
    dbgWaveHitPlayer_ = false;
    dbgPlayerGuardedHit_ = false;

    const auto swords = player_.GetSwords();
    const auto swordSlashStates = player_.GetSwordSlashStates();

    for (size_t i = 0; i < swords.size(); ++i) {
        const Sword *sword = swords[i];
        if (sword == nullptr || !swordSlashStates[i]) {
            continue;
        }

        auto swordHitBox = sword->GetOBB();
        auto bodyBox = enemy_.GetBodyOBB();
        auto leftHandBox = enemy_.GetLeftHandOBB();
        auto rightHandBox = enemy_.GetRightHandOBB();

        bool hitLeftHand = CollisionUtil::CheckOBB(swordHitBox, leftHandBox);
        bool hitRightHand = CollisionUtil::CheckOBB(swordHitBox, rightHandBox);
        bool hitBody = CollisionUtil::CheckOBB(swordHitBox, bodyBox);

        dbgHitLeftHand_ = dbgHitLeftHand_ || hitLeftHand;
        dbgHitRightHand_ = dbgHitRightHand_ || hitRightHand;
        dbgHitBody_ = dbgHitBody_ || hitBody;

        if (enemyHitCooldown_ <= 0.0f) {
            if (hitBody) {
                enemy_.TakeDamage(10.0f);
                RequestEffect(EffectParticlePreset::HitSpark,
                              bodyBox.center);
                enemyHitCooldown_ = 0.2f;
                if (counterCinematicActive_) {
                    stopCounterCinematicThisFrame = true;
                }
            }
        }

        if (enemyHitCooldown_ > 0.0f) {
            break;
        }
    }

    const bool isEnemySmashCounterWindow =
        (enemyActionKind == ActionKind::Smash &&
         enemyActionStep == ActionStep::Active);
    const bool isEnemySweepCounterWindow =
        (enemyActionKind == ActionKind::Sweep &&
         enemyActionStep == ActionStep::Active);
    const bool isEnemySmashMeleeWindow =
        (enemyActionKind == ActionKind::Smash &&
         (enemyActionStep == ActionStep::Active ||
          enemyActionStep == ActionStep::Recovery));
    const bool isEnemySweepMeleeWindow =
        (enemyActionKind == ActionKind::Sweep &&
         (enemyActionStep == ActionStep::Active ||
          enemyActionStep == ActionStep::Recovery));
    const bool isEnemyMeleeActive =
        isEnemySmashMeleeWindow || isEnemySweepMeleeWindow;

    const float enemyAttackDamage = enemy_.GetCurrentAttackDamage();
    const float enemyAttackKnockback = enemy_.GetCurrentAttackKnockback();
    const bool isCounterAxisMatch =
        (isEnemySmashCounterWindow &&
         player_.GetCounterAxis() == SwordCounterAxis::Vertical) ||
        (isEnemySweepCounterWindow &&
         player_.GetCounterAxis() == SwordCounterAxis::Horizontal);
    const bool canCounterThisHit =
        player_.IsCounterStance() && isCounterAxisMatch;

    bool bossHitPlayer = false;

    if (!freezeEnemyMotion && isEnemyMeleeActive) {
        auto enemyAttackBox = enemy_.GetAttackOBB();
        bossHitPlayer = CollisionUtil::CheckOBB(enemyAttackBox, playerBox);

        if (bossHitPlayer && playerHitCooldown_ <= 0.0f) {
            float dx = player_.GetTransform().position.x -
                       enemy_.GetTransform().position.x;
            float dz = player_.GetTransform().position.z -
                       enemy_.GetTransform().position.z;
            float len = std::sqrt(dx * dx + dz * dz);
            if (len < 0.0001f) {
                len = 1.0f;
            }

            dx /= len;
            dz /= len;

            if (canCounterThisHit) {
                triggerSuccessfulCounter(enemyAttackDamage, 0.2f);
                bossHitPlayer = false;
            } else if (isPlayerGuarding) {
                dbgPlayerGuardedHit_ = true;
                enemy_.NotifyAttackGuarded();
                player_.TakeDamage(enemyAttackDamage * kGuardDamageMultiplier);
                RequestEffect(EffectParticlePreset::HitSpark,
                              player_.GetTransform().position);
                player_.AddKnockback({dx * (enemyAttackKnockback * 0.5f), 0.0f,
                                      dz * (enemyAttackKnockback * 0.5f)});
                playerHitCooldown_ = 0.2f;
            } else {
                enemy_.NotifyAttackConnected();
                player_.TakeDamage(enemyAttackDamage);
                RequestEffect(EffectParticlePreset::HitSpark,
                              player_.GetTransform().position);
                player_.AddKnockback({dx * enemyAttackKnockback, 0.0f,
                                      dz * enemyAttackKnockback});
                playerHitCooldown_ = 0.4f;
            }
        }
    }

    dbgBossHitPlayer_ = bossHitPlayer;
    dbgBulletHitPlayer_ = false;

    if (freezeEnemyMotion) {
        if (startCounterCinematicThisFrame) {
            counterCinematicActive_ = true;
            SetEnemyAnimationFrozen(true);
        }
        if (stopCounterCinematicThisFrame) {
            counterCinematicActive_ = false;
            SetEnemyAnimationFrozen(false);
        }
        UpdateCounterVignette(baseDeltaTime);
        return;
    }

    const auto &bullets = enemy_.GetBullets();
    for (size_t i = 0; i < bullets.size(); ++i) {
        const auto &bullet = bullets[i];
        if (!bullet.isAlive) {
            continue;
        }

        OBB bulletBox{};
        bulletBox.center = bullet.position;
        bulletBox.size = enemy_.GetBulletHitBoxSize();
        bulletBox.rotation = player_.GetTransform().rotation;

        if (!bullet.isReflected && isPlayerCountering &&
            CollisionUtil::CheckOBB(bulletBox, counterBox)) {
            enemy_.ReflectBullet(i, enemy_.GetTransform().position);
            dbgBulletHitPlayer_ = false;
            continue;
        }

        if (bullet.isReflected) {
            if (CollisionUtil::CheckOBB(bulletBox, enemyBodyBox) &&
                enemyHitCooldown_ <= 0.0f) {
                reflectDamage_ = enemy_.GetBulletDamage() * damageMultiplier_;
                enemy_.TakeDamage(reflectDamage_);
                RequestEffect(EffectParticlePreset::HitSpark,
                              bullet.position);
                enemy_.DestroyBullet(i);
                enemyHitCooldown_ = 0.2f;
            }
            continue;
        }

        if (CollisionUtil::CheckOBB(bulletBox, playerBox)) {
            dbgBulletHitPlayer_ = true;

            if (playerHitCooldown_ <= 0.0f) {
                float vx = bullet.velocity.x;
                float vz = bullet.velocity.z;
                float len = std::sqrt(vx * vx + vz * vz);
                if (len < 0.0001f) {
                    len = 1.0f;
                }

                vx /= len;
                vz /= len;

                if (player_.IsCounterStance()) {
                    triggerSuccessfulCounter(enemy_.GetBulletDamage() * 2.0f,
                                             0.12f);
                } else if (isPlayerGuarding) {
                    dbgPlayerGuardedHit_ = true;
                    player_.TakeDamage(enemy_.GetBulletDamage() *
                                       kGuardDamageMultiplier);
                    RequestEffect(EffectParticlePreset::HitSpark,
                                  bullet.position);
                    player_.AddKnockback(
                        {vx * (enemy_.GetBulletKnockback() * 0.5f), 0.0f,
                         vz * (enemy_.GetBulletKnockback() * 0.5f)});
                    enemy_.DestroyBullet(i);
                    playerHitCooldown_ = 0.15f;
                } else {
                    player_.TakeDamage(enemy_.GetBulletDamage());
                    RequestEffect(EffectParticlePreset::HitSpark,
                                  bullet.position);
                    player_.AddKnockback({vx * enemy_.GetBulletKnockback(),
                                          0.0f,
                                          vz * enemy_.GetBulletKnockback()});
                    enemy_.DestroyBullet(i);
                    playerHitCooldown_ = 0.3f;
                }
            }

            enemy_.ConsumeBullet(i);
            break;
        }
    }

    dbgWaveHitPlayer_ = false;

    const auto &waves = enemy_.GetWaves();
    for (size_t i = 0; i < waves.size(); ++i) {
        const auto &wave = waves[i];
        if (!wave.isAlive) {
            continue;
        }

        OBB waveBox{};
        waveBox.center = wave.position;
        waveBox.size = enemy_.GetWaveHitBoxSize();
        waveBox.rotation = player_.GetTransform().rotation;

        if (!wave.isReflected && isPlayerCountering &&
            CollisionUtil::CheckOBB(waveBox, counterBox)) {
            enemy_.ReflectWave(i, enemy_.GetTransform().position);
            dbgWaveHitPlayer_ = false;
            continue;
        }

        if (wave.isReflected) {
            if (CollisionUtil::CheckOBB(waveBox, enemyBodyBox) &&
                enemyHitCooldown_ <= 0.0f) {
                reflectDamage_ = enemy_.GetWaveDamage() * damageMultiplier_;
                enemy_.TakeDamage(reflectDamage_);
                RequestEffect(EffectParticlePreset::HitSpark,
                              wave.position);
                enemy_.DestroyWave(i);
                enemyHitCooldown_ = 0.2f;
            }
            continue;
        }

        if (CollisionUtil::CheckOBB(waveBox, playerBox)) {
            dbgWaveHitPlayer_ = true;

            if (playerHitCooldown_ <= 0.0f) {
                float vx = wave.direction.x;
                float vz = wave.direction.z;
                float len = std::sqrt(vx * vx + vz * vz);
                if (len < 0.0001f) {
                    len = 1.0f;
                }

                vx /= len;
                vz /= len;

                if (player_.IsCounterStance()) {
                    triggerSuccessfulCounter(enemy_.GetWaveDamage() * 2.0f,
                                             0.12f);
                } else if (isPlayerGuarding) {
                    dbgPlayerGuardedHit_ = true;
                    player_.TakeDamage(enemy_.GetWaveDamage() *
                                       kGuardDamageMultiplier);
                    RequestEffect(EffectParticlePreset::HitSpark,
                                  wave.position);
                    player_.AddKnockback(
                        {vx * (enemy_.GetWaveKnockback() * 0.5f), 0.0f,
                         vz * (enemy_.GetWaveKnockback() * 0.5f)});
                    enemy_.DestroyWave(i);
                    playerHitCooldown_ = 0.15f;
                } else {
                    player_.TakeDamage(enemy_.GetWaveDamage());
                    RequestEffect(EffectParticlePreset::HitSpark,
                                  wave.position);
                    player_.AddKnockback({vx * enemy_.GetWaveKnockback(), 0.0f,
                                          vz * enemy_.GetWaveKnockback()});
                    enemy_.DestroyWave(i);
                    playerHitCooldown_ = 0.35f;
                }
            }

            enemy_.ConsumeWave(i);
            break;
        }
    }

    if (startCounterCinematicThisFrame) {
        counterCinematicActive_ = true;
        SetEnemyAnimationFrozen(true);
    }
    if (stopCounterCinematicThisFrame) {
        counterCinematicActive_ = false;
        SetEnemyAnimationFrozen(false);
    }
    if (forceSyncEnemyAnimationThisFrame) {
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
// #ifdef _DEBUG
//     DebugDraw *debugDraw = ctx_->debugDraw;
// #endif

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
    int aliveBulletCount = 0;
    for (const auto &bullet : enemy_.GetBullets()) {
        if (bullet.isAlive) {
            aliveBulletCount++;
        }
    }

    int aliveWaveCount = 0;
    for (const auto &wave : enemy_.GetWaves()) {
        if (wave.isAlive) {
            aliveWaveCount++;
        }
    }
#ifdef _DEBUG
    if (EngineRuntime::GetInstance().Settings().showDebugUI &&
        !EngineRuntime::GetInstance().IsEditorMode()) {
    // 蠖薙◁E��雁�E螳壽緒逕ｻ
    ModelDrawEffect hitBoxEffect{};
    hitBoxEffect.enabled = true;
    hitBoxEffect.intensity = 0.45f;
    hitBoxEffect.fresnelPower = 2.8f;
    hitBoxEffect.noiseAmount = 0.06f;
    hitBoxEffect.time = sceneLightTime_ * 5.0f;

    // 繝励Ξ繧�E�繝､繝ｼ譛ｬ菴・
    hitBoxEffect.color = {0.20f, 0.95f, 0.28f, 0.45f};
    ctx_->model->SetDrawEffect(hitBoxEffect);
    ctx_->debugDraw->DrawOBB(ctx_->model, player_.GetOBB(), *currentCamera);

    // 繝励Ξ繧�E�繝､繝ｼ蜑｣
    hitBoxEffect.color = {0.20f, 0.85f, 1.00f, 0.42f};
    ctx_->model->SetDrawEffect(hitBoxEffect);
    for (const Sword *sword : player_.GetSwords()) {
        if (sword == nullptr) {
            continue;
        }
        ctx_->debugDraw->DrawOBB(ctx_->model, sword->GetOBB(), *currentCamera);
    }

    // 繝懊せ驛ｨ菴・
    if (enemy_.IsAlive()) {
        hitBoxEffect.color = {1.00f, 0.28f, 0.20f, 0.40f};
        ctx_->model->SetDrawEffect(hitBoxEffect);
        ctx_->debugDraw->DrawOBB(ctx_->model, enemy_.GetBodyOBB(),
                                 *currentCamera);
        hitBoxEffect.color = {1.00f, 0.45f, 0.25f, 0.35f};
        ctx_->model->SetDrawEffect(hitBoxEffect);
        ctx_->debugDraw->DrawOBB(ctx_->model, enemy_.GetLeftHandOBB(),
                                 *currentCamera);
        ctx_->debugDraw->DrawOBB(ctx_->model, enemy_.GetRightHandOBB(),
                                 *currentCamera);

        const bool isEnemySmashActive =
            (enemy_.GetActionKind() == ActionKind::Smash &&
             (enemy_.GetActionStep() == ActionStep::Active ||
              enemy_.GetActionStep() == ActionStep::Recovery));
        const bool isEnemySweepActive =
            (enemy_.GetActionKind() == ActionKind::Sweep &&
             (enemy_.GetActionStep() == ActionStep::Active ||
              enemy_.GetActionStep() == ActionStep::Recovery));

        if (isEnemySmashActive || isEnemySweepActive) {
            hitBoxEffect.color = {1.00f, 1.00f, 0.15f, 0.52f};
            hitBoxEffect.intensity = 0.62f;
            ctx_->model->SetDrawEffect(hitBoxEffect);
            ctx_->debugDraw->DrawOBB(ctx_->model, enemy_.GetAttackOBB(),
                                     *currentCamera);
            hitBoxEffect.intensity = 0.45f;
        }

        // 蠑ｾ繝�Eャ繝亥愛螳・
        hitBoxEffect.color = {0.95f, 0.20f, 1.00f, 0.34f};
        ctx_->model->SetDrawEffect(hitBoxEffect);
        for (const auto &bullet : enemy_.GetBullets()) {
            if (!bullet.isAlive) {
                continue;
            }

            OBB bulletBox{};
            bulletBox.center = bullet.position;
            bulletBox.size = enemy_.GetBulletHitBoxSize();
            bulletBox.rotation = player_.GetTransform().rotation;
            ctx_->debugDraw->DrawOBB(ctx_->model, bulletBox, *currentCamera);
        }

        // 豕｢蜍輔ヲ繝�Eヨ蛻�E�螳・
        hitBoxEffect.color = {0.25f, 0.65f, 1.00f, 0.34f};
        ctx_->model->SetDrawEffect(hitBoxEffect);
        for (const auto &wave : enemy_.GetWaves()) {
            if (!wave.isAlive) {
                continue;
            }

            OBB waveBox{};
            waveBox.center = wave.position;
            waveBox.size = enemy_.GetWaveHitBoxSize();
            waveBox.rotation = player_.GetTransform().rotation;
            ctx_->debugDraw->DrawOBB(ctx_->model, waveBox, *currentCamera);
        }
    }
    ctx_->model->ClearDrawEffect();
    }
#endif // _DEBUG
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

bool GameScene::ComputeEnemySlashScreenEffect(XMFLOAT2 &outStart,
                                              XMFLOAT2 &outEnd,
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
         actionStep == ActionStep::Active || actionStep == ActionStep::Recovery);
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

    if (!ProjectWorldToScreen(slashStartWorld, outStart) ||
        !ProjectWorldToScreen(slashEndWorld, outEnd)) {
        return false;
    }

    const AttackTimingParam *timing = enemy_.GetCurrentAttackTimingPublic();
    const float actionTime = enemy_.GetCurrentActionTimePublic();
    float phaseAlpha = 0.0f;
    if (actionStep == ActionStep::Charge || actionStep == ActionStep::Hold) {
        const float previewT =
            (timing != nullptr && timing->activeStartTime > 0.0001f)
                ? actionTime / timing->activeStartTime
                : 1.0f;
        phaseAlpha =
            enemySlashChargePreviewAlpha_ * (0.35f + 0.65f * EaseOutCubic(previewT));
    } else if (actionStep == ActionStep::Active) {
        float activeT = 0.0f;
        if (timing != nullptr && timing->activeEndTime > timing->activeStartTime) {
            activeT = (actionTime - timing->activeStartTime) /
                      (timing->activeEndTime - timing->activeStartTime);
        }
        phaseAlpha = enemySlashActiveAlpha_ * (1.0f - 0.22f * Clamp01(activeT));
    } else {
        float recoveryT = 0.0f;
        if (timing != nullptr && timing->totalTime > timing->recoveryStartTime) {
            recoveryT = (actionTime - timing->recoveryStartTime) /
                        (timing->totalTime - timing->recoveryStartTime);
        }
        phaseAlpha =
            enemySlashRecoveryAlpha_ * (1.0f - EaseOutCubic(recoveryT));
    }

    if (phaseAlpha <= 0.01f) {
        return false;
    }

    outPhaseAlpha = phaseAlpha;
    outActionTime = actionTime;
    outActionKind = actionKind;
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
    prevEnemyActionKind_ = actionKind;
    prevEnemyActionStep_ = actionStep;
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

