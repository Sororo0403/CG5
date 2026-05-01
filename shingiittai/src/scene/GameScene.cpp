#include "GameScene.h"
#include "CollisionUtil.h"
#include "DirectXCommon.h"
#include "EnemyMotionDebugScene.h"
#include "EngineRuntime.h"
#include "Input.h"
#include "ModelManager.h"
#include "PostEffectRenderer.h"
#include "SceneManager.h"
#include "PlayerTuningPresetIO.h"
#include "SpriteManager.h"
#include "TextureManager.h"
#include "WinApp.h"
#include "WarpPostEffectRenderer.h"
#include "ElectricRingEffectRenderer.h"
#include "GpuSlashParticleSystem.h"
#include "SlashEffectRenderer.h"
#include "TrailRenderer.h"
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

#ifndef IMGUI_DISABLED
static ImU32 ToImColor(const XMFLOAT4 &color, float alphaScale = 1.0f) {
    int r = static_cast<int>(Clamp01(color.x) * 255.0f);
    int g = static_cast<int>(Clamp01(color.y) * 255.0f);
    int b = static_cast<int>(Clamp01(color.z) * 255.0f);
    int a = static_cast<int>(Clamp01(color.w * alphaScale) * 255.0f);
    return IM_COL32(r, g, b, a);
}
#endif

void GameScene::Initialize(const SceneContext &ctx) {
    BaseScene::Initialize(ctx);

    float aspect = static_cast<float>(ctx_->winApp->GetWidth()) /
                   static_cast<float>(ctx_->winApp->GetHeight());

    camera_.Initialize(aspect);
    camera_.SetMode(CameraMode::LookAt);
    camera_.UpdateMatrices();
    camera_.SetPerspectiveFovDeg(currentFovDeg_);
#ifdef _DEBUG
    debugCamera_.Initialize(aspect);
    debugCamera_.SetMode(CameraMode::Free);
    debugCamera_.UpdateMatrices();

    tripodCamera_.Initialize(aspect);
    camera_.SetMode(CameraMode::LookAt);
    tripodCamera_.SetPosition(tripodPos_);
    tripodCamera_.LookAt(tripodTarget_);
    tripodCamera_.UpdateMatrices();
#endif

    currentCamera_ = &camera_;

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

    // 荳莠�E�遘ｰ繧�E�繝｡繝ｩ蛻晁E��蜷代″
    cameraYaw_ = 0.0f;
    cameraPitch_ = 0.0f;
    isLockOn_ = true;
    rushChargeAssistStrength_ = 4.0f;
    rushChargeAssistMaxStep_ = 6.0f;
    rushActiveAssistStrength_ = 5.0f;
    rushActiveAssistMaxStep_ = 8.0f;
    rushLeadDistance_ = 2.5f;
    currentFovDeg_ = normalFovDeg_;
    targetFovDeg_ = normalFovDeg_;

    // 閧�E�雜翫�E�荳我ｺ�E�遘ｰ繧�E�繝｡繝ｩ蛻晁E��蜷代″
    const DirectX::XMFLOAT3 &playerPos = player_.GetTransform().position;
    const DirectX::XMFLOAT3 &enemyPos = enemy_.GetTransform().position;
    lockOnOrbitCameraPos_ = {playerPos.x, playerPos.y + lockOnOrbitHeight_,
                             playerPos.z - lockOnOrbitRadius_};
    lockOnLookAt_ = {
        playerPos.x * lockOnLookPlayerWeight_ +
            enemyPos.x * lockOnLookEnemyWeight_,
        (playerPos.y + cameraLookHeight_) * 0.52f +
            (enemyPos.y + 1.30f) * 0.48f,
        playerPos.z * lockOnLookPlayerWeight_ +
            enemyPos.z * lockOnLookEnemyWeight_};
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
#ifdef _DEBUG
    if (input != nullptr && input->IsKeyTrigger(DIK_F8) &&
        sceneManager_ != nullptr) {
        sceneManager_->ChangeScene(std::make_unique<EnemyMotionDebugScene>());
        return;
    }
#endif

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

            UpdateBattleCamera();
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

        UpdateBattleCamera();
        counterCinematicActive_ = false;
        UpdateCounterVignette(baseDeltaTime);
        return;
    }

    ctx_->model->UpdateAnimation(playerModelId_, playerDeltaTime);
#ifdef _DEBUG
    if (currentCamera_ == &debugCamera_) {
        return;
    }
#endif

    // 蠖薙◁E��雁�E螳・
    // 蜈医↓繝励Ξ繧�E�繝､繝ｼ繧呈峩譁E��縺励※縲√◎縺�E�邨先棡繧脱nemy縺�E�貂｡縺・
    player_.Update(input, playerDeltaTime, enemy_.GetTransform().position,
                   cameraYaw_);
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

    UpdateMagnetismic();

    UpdateBattleCamera();

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
        RequestEffect(GpuSlashParticleSystem::EffectPreset::HitSpark,
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
                RequestEffect(GpuSlashParticleSystem::EffectPreset::HitSpark,
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
                RequestEffect(GpuSlashParticleSystem::EffectPreset::HitSpark,
                              player_.GetTransform().position);
                player_.AddKnockback({dx * (enemyAttackKnockback * 0.5f), 0.0f,
                                      dz * (enemyAttackKnockback * 0.5f)});
                playerHitCooldown_ = 0.2f;
            } else {
                enemy_.NotifyAttackConnected();
                player_.TakeDamage(enemyAttackDamage);
                RequestEffect(GpuSlashParticleSystem::EffectPreset::HitSpark,
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
                RequestEffect(GpuSlashParticleSystem::EffectPreset::HitSpark,
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
                    RequestEffect(GpuSlashParticleSystem::EffectPreset::HitSpark,
                                  bullet.position);
                    player_.AddKnockback(
                        {vx * (enemy_.GetBulletKnockback() * 0.5f), 0.0f,
                         vz * (enemy_.GetBulletKnockback() * 0.5f)});
                    enemy_.DestroyBullet(i);
                    playerHitCooldown_ = 0.15f;
                } else {
                    player_.TakeDamage(enemy_.GetBulletDamage());
                    RequestEffect(GpuSlashParticleSystem::EffectPreset::HitSpark,
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
                RequestEffect(GpuSlashParticleSystem::EffectPreset::HitSpark,
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
                    RequestEffect(GpuSlashParticleSystem::EffectPreset::HitSpark,
                                  wave.position);
                    player_.AddKnockback(
                        {vx * (enemy_.GetWaveKnockback() * 0.5f), 0.0f,
                         vz * (enemy_.GetWaveKnockback() * 0.5f)});
                    enemy_.DestroyWave(i);
                    playerHitCooldown_ = 0.15f;
                } else {
                    player_.TakeDamage(enemy_.GetWaveDamage());
                    RequestEffect(GpuSlashParticleSystem::EffectPreset::HitSpark,
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

    if (ctx_ != nullptr && ctx_->warpPostEffectParam != nullptr) {
        WarpPostEffectParamGPU *warpParam = ctx_->warpPostEffectParam;

        warpParam->time += ctx_->deltaTime;
        warpParam->center = {0.5f, 0.5f};
        warpParam->radius = 0.0f;
        warpParam->strength = 0.0f;
        warpParam->center2 = {0.5f, 0.5f};
        warpParam->radius2 = 0.0f;
        warpParam->strength2 = 0.0f;
        warpParam->enabled = 0.0f;
        warpParam->slashStart = {0.5f, 0.5f};
        warpParam->slashEnd = {0.5f, 0.5f};
        warpParam->slashThickness = 0.0f;
        warpParam->slashStrength = 0.0f;
        warpParam->slashEnabled = 0.0f;

        XMFLOAT2 slashStartScreen{};
        XMFLOAT2 slashEndScreen{};
        float slashPhaseAlpha = 0.0f;
        float slashActionTime = 0.0f;
        ActionKind slashActionKind = ActionKind::None;
        if (ComputeEnemySlashScreenEffect(slashStartScreen, slashEndScreen,
                                          slashPhaseAlpha, slashActionTime,
                                          slashActionKind)) {
            const float width = static_cast<float>(ctx_->winApp->GetWidth());
            const float height = static_cast<float>(ctx_->winApp->GetHeight());
            warpParam->slashStart = {slashStartScreen.x / width,
                                     slashStartScreen.y / height};
            warpParam->slashEnd = {slashEndScreen.x / width,
                                   slashEndScreen.y / height};
            warpParam->slashThickness =
                enemySlashDistortionThicknessUv_ *
                (slashActionKind == ActionKind::Sweep ? 1.18f : 1.0f) *
                (0.72f + slashPhaseAlpha * 0.55f);
            warpParam->slashStrength =
                enemySlashDistortionStrength_ * (0.58f + slashPhaseAlpha * 0.88f);
            warpParam->slashEnabled = 1.0f;
        }

        if (enemy_.GetActionKind() == ActionKind::Warp) {
            const float width = static_cast<float>(ctx_->winApp->GetWidth());
            const float height = static_cast<float>(ctx_->winApp->GetHeight());

            bool hasAnyCenter = false;
            DirectX::XMFLOAT2 targetScreen{};
            if (ProjectWorldToScreen(enemy_.GetWarpTargetPos(), targetScreen)) {
                warpParam->center = {targetScreen.x / width,
                                     targetScreen.y / height};
                hasAnyCenter = true;
            }

            if (enemy_.HasWarpDeparturePos()) {
                DirectX::XMFLOAT2 sourceScreen{};
                if (ProjectWorldToScreen(enemy_.GetWarpDeparturePos(),
                                         sourceScreen)) {
                    warpParam->center2 = {sourceScreen.x / width,
                                          sourceScreen.y / height};
                }
            }

            if (hasAnyCenter) {
                warpParam->enabled = 1.0f;

                switch (enemy_.GetActionStep()) {
                case ActionStep::Start:
                    warpParam->radius = 0.085f;
                    warpParam->strength = 0.010f;
                    warpParam->radius2 = 0.110f;
                    warpParam->strength2 = 0.014f;
                    break;
                case ActionStep::Move:
                    warpParam->radius = 0.060f;
                    warpParam->strength = 0.004f;
                    warpParam->radius2 = 0.085f;
                    warpParam->strength2 = 0.007f;
                    break;
                case ActionStep::End:
                    warpParam->radius = 0.135f;
                    warpParam->strength = 0.022f;
                    warpParam->radius2 = 0.040f;
                    warpParam->strength2 = 0.004f;
                    break;
                default:
                    warpParam->enabled = 0.0f;
                    break;
                }
            }
        }
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
    if (ctx_ == nullptr || ctx_->renderer.postEffectRenderer == nullptr ||
        currentCamera_ == nullptr) {
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

    const bool playerVisible =
        editableObjects_.size() < 1 || editableObjects_[0].visible;
    const bool enemyVisible =
        editableObjects_.size() < 2 || editableObjects_[1].visible;
    if (playerVisible) {
        player_.Draw(ctx_->model, *currentCamera_);
    }
    if (enemyVisible) {
        enemy_.Draw(ctx_->model, *currentCamera_);
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
    ctx_->debugDraw->DrawOBB(ctx_->model, player_.GetOBB(), *currentCamera_);

    // 繝励Ξ繧�E�繝､繝ｼ蜑｣
    hitBoxEffect.color = {0.20f, 0.85f, 1.00f, 0.42f};
    ctx_->model->SetDrawEffect(hitBoxEffect);
    for (const Sword *sword : player_.GetSwords()) {
        if (sword == nullptr) {
            continue;
        }
        ctx_->debugDraw->DrawOBB(ctx_->model, sword->GetOBB(), *currentCamera_);
    }

    // 繝懊せ驛ｨ菴・
    if (enemy_.IsAlive()) {
        hitBoxEffect.color = {1.00f, 0.28f, 0.20f, 0.40f};
        ctx_->model->SetDrawEffect(hitBoxEffect);
        ctx_->debugDraw->DrawOBB(ctx_->model, enemy_.GetBodyOBB(),
                                 *currentCamera_);
        hitBoxEffect.color = {1.00f, 0.45f, 0.25f, 0.35f};
        ctx_->model->SetDrawEffect(hitBoxEffect);
        ctx_->debugDraw->DrawOBB(ctx_->model, enemy_.GetLeftHandOBB(),
                                 *currentCamera_);
        ctx_->debugDraw->DrawOBB(ctx_->model, enemy_.GetRightHandOBB(),
                                 *currentCamera_);

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
                                     *currentCamera_);
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
            ctx_->debugDraw->DrawOBB(ctx_->model, bulletBox, *currentCamera_);
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
            ctx_->debugDraw->DrawOBB(ctx_->model, waveBox, *currentCamera_);
        }
    }
    ctx_->model->ClearDrawEffect();
    }
#endif // _DEBUG
    ctx_->model->PostDraw();
    if (EngineRuntime::GetInstance().IsEditorMode()) {
        return;
    }
    DrawMagnetismic();
    if (ctx_->trailRenderer != nullptr) {
        ctx_->trailRenderer->Draw(*currentCamera_);
    }

    {
        DirectX::XMFLOAT3 slashStartWorld{};
        DirectX::XMFLOAT3 slashEndWorld{};
        float phaseAlpha = 0.0f;
        float actionTime = 0.0f;
        ActionKind actionKind = ActionKind::None;

        if (ComputeEnemySlashWorldEffect(slashStartWorld, slashEndWorld,
                                         phaseAlpha, actionTime, actionKind)) {
            const bool isSweep = (actionKind == ActionKind::Sweep);
          
            ctx_->slashEffectRenderer->DrawEnemySlash(
                *currentCamera_, slashStartWorld, slashEndWorld, phaseAlpha,
                actionTime, isSweep);
        }
    }
    DrawEnemySlashPass();

    ctx_->gpuSlashParticleSystem->Render(*currentCamera_, ctx_->deltaTime);
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
    if (currentCamera_ == nullptr || ctx_ == nullptr || ctx_->winApp == nullptr) {
        return false;
    }

    XMMATRIX viewProj = currentCamera_->GetView() * currentCamera_->GetProj();
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

void GameScene::DrawEnemySlashPass() {
#ifdef IMGUI_DISABLED
    return;
#else
    ImGuiContext *imguiCtx = ImGui::GetCurrentContext();
    if (imguiCtx == nullptr || imguiCtx->Viewports.Size <= 0) {
        return;
    }

    ImGuiViewport *mainViewport = ImGui::GetMainViewport();
    if (mainViewport == nullptr) {
        return;
    }

    ImDrawList *drawList = ImGui::GetBackgroundDrawList(mainViewport);
    if (drawList == nullptr) {
        return;
    }

    XMFLOAT2 slashStartScreenF{};
    XMFLOAT2 slashEndScreenF{};
    float phaseAlpha = 0.0f;
    float actionTime = 0.0f;
    ActionKind actionKind = ActionKind::None;
    if (!ComputeEnemySlashScreenEffect(slashStartScreenF, slashEndScreenF,
                                       phaseAlpha, actionTime, actionKind)) {
        return;
    }

    ImVec2 start(slashStartScreenF.x, slashStartScreenF.y);
    ImVec2 end(slashEndScreenF.x, slashEndScreenF.y);
    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float length = std::sqrtf(dx * dx + dy * dy);
    if (length <= 0.0001f) {
        return;
    }

    float invLen = 1.0f / length;
    float dirX = dx * invLen;
    float dirY = dy * invLen;
    float perpX = -dirY;
    float perpY = dirX;

    float sweepCurve = (actionKind == ActionKind::Sweep) ? 52.0f : 30.0f;
    float pulse = 0.84f + 0.16f * std::sinf(actionTime * 36.0f);
    float curveOffset = sweepCurve * pulse;
    ImVec2 control((start.x + end.x) * 0.5f + perpX * curveOffset,
                   (start.y + end.y) * 0.5f + perpY * curveOffset);

    constexpr int kSlashSegments = 18;
    ImVec2 path[kSlashSegments + 1];
    const float outerThickness =
        enemySlashOuterThicknessPx_ *
        (actionKind == ActionKind::Sweep ? 1.15f : 1.0f) *
        (0.86f + phaseAlpha * 0.28f);
    const float coreThickness =
        enemySlashCoreThicknessPx_ * (0.82f + phaseAlpha * 0.24f);
    float widths[kSlashSegments + 1];
    for (int i = 0; i <= kSlashSegments; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(kSlashSegments);
        float inv = 1.0f - t;
        path[i].x = inv * inv * start.x + 2.0f * inv * t * control.x + t * t * end.x;
        path[i].y = inv * inv * start.y + 2.0f * inv * t * control.y + t * t * end.y;
        float tipTaper = 1.0f - std::fabs(t - 0.52f) * 1.35f;
        tipTaper = Clamp01(tipTaper);
        widths[i] = outerThickness * (0.25f + 0.75f * tipTaper);
    }

    XMFLOAT4 shadowColor = {0.03f, 0.01f, 0.02f, phaseAlpha * 0.96f};
    XMFLOAT4 darkRimColor = {0.17f, 0.01f, 0.04f, phaseAlpha * 0.82f};
    XMFLOAT4 hotColor = {0.96f, 0.12f, 0.30f, phaseAlpha * 0.68f};
    XMFLOAT4 coreColor = {1.00f, 0.95f, 0.97f, phaseAlpha * 0.94f};
    XMFLOAT4 bloomColor = {0.74f, 0.58f, 1.00f, phaseAlpha * 0.44f};

    ImVec2 ribbon[(kSlashSegments + 1) * 2];
    for (int i = 0; i <= kSlashSegments; ++i) {
        int prevIndex = (i == 0) ? i : i - 1;
        int nextIndex = (i == kSlashSegments) ? i : i + 1;
        float tangentX = path[nextIndex].x - path[prevIndex].x;
        float tangentY = path[nextIndex].y - path[prevIndex].y;
        float tangentLen =
            std::sqrtf(tangentX * tangentX + tangentY * tangentY);
        if (tangentLen <= 0.0001f) {
            tangentX = dirX;
            tangentY = dirY;
            tangentLen = 1.0f;
        }
        tangentX /= tangentLen;
        tangentY /= tangentLen;
        float normalX = -tangentY;
        float normalY = tangentX;
        float width = widths[i];
        ribbon[i] = ImVec2(path[i].x + normalX * width,
                           path[i].y + normalY * width);
        ribbon[(kSlashSegments + 1) * 2 - 1 - i] =
            ImVec2(path[i].x - normalX * width * 0.65f,
                   path[i].y - normalY * width * 0.65f);
    }

    ImVec2 echoPath[kSlashSegments + 1];
    for (int i = 0; i <= kSlashSegments; ++i) {
        echoPath[i] = ImVec2(path[i].x + perpX * enemySlashEchoOffsetPx_,
                             path[i].y + perpY * enemySlashEchoOffsetPx_);
    }

    drawList->AddConvexPolyFilled(ribbon, (kSlashSegments + 1) * 2,
                                  ToImColor(shadowColor, 0.46f));
    drawList->AddPolyline(path, kSlashSegments + 1, ToImColor(bloomColor, 0.80f),
                          false, outerThickness * 1.55f);
    drawList->AddPolyline(echoPath, kSlashSegments + 1,
                          ToImColor(shadowColor, 0.55f), false,
                          outerThickness * 1.05f);
    drawList->AddPolyline(path, kSlashSegments + 1, ToImColor(shadowColor), false,
                          outerThickness);
    drawList->AddPolyline(path, kSlashSegments + 1, ToImColor(darkRimColor), false,
                          outerThickness * 0.72f);
    drawList->AddPolyline(path, kSlashSegments + 1, ToImColor(hotColor), false,
                          outerThickness * 0.34f);
    drawList->AddPolyline(path, kSlashSegments + 1, ToImColor(coreColor), false,
                          coreThickness);

    const ImVec2 impact = path[kSlashSegments];
    const float branchScale = (actionKind == ActionKind::Sweep) ? 0.82f : 0.68f;
    const float branchLen = length * branchScale * 0.22f;
    const float branchForward = length * 0.08f;

    ImVec2 branchA0(impact.x - perpX * branchLen, impact.y - perpY * branchLen);
    ImVec2 branchA1(impact.x + perpX * branchLen + dirX * branchForward,
                    impact.y + perpY * branchLen + dirY * branchForward);
    ImVec2 branchB0(impact.x + perpX * branchLen * 0.52f - dirX * branchForward * 0.7f,
                    impact.y + perpY * branchLen * 0.52f - dirY * branchForward * 0.7f);
    ImVec2 branchB1(impact.x - perpX * branchLen * 0.78f + dirX * branchForward * 0.5f,
                    impact.y - perpY * branchLen * 0.78f + dirY * branchForward * 0.5f);

    drawList->AddLine(branchA0, branchA1, ToImColor(darkRimColor), outerThickness * 0.22f);
    drawList->AddLine(branchA0, branchA1, ToImColor(coreColor), coreThickness * 0.46f);
    drawList->AddLine(branchB0, branchB1, ToImColor(hotColor), coreThickness * 0.38f);

    constexpr int kInkSpikeCount = 5;
    for (int i = 0; i < kInkSpikeCount; ++i) {
        float ratio = static_cast<float>(i) / static_cast<float>(kInkSpikeCount - 1);
        int pathIndex = static_cast<int>(ratio * static_cast<float>(kSlashSegments));
        float spikeScale = (0.45f + 0.55f * (1.0f - ratio)) *
                           (actionKind == ActionKind::Sweep ? 1.22f : 1.0f);
        float spikeLen = length * 0.12f * spikeScale;
        float sweepBias = std::sinf(actionTime * 18.0f + ratio * 7.0f) * 18.0f;
        ImVec2 spikeStart(path[pathIndex].x + dirX * (ratio * 22.0f),
                          path[pathIndex].y + dirY * (ratio * 22.0f));
        ImVec2 spikeEnd(spikeStart.x - perpX * (spikeLen + sweepBias),
                        spikeStart.y - perpY * (spikeLen + sweepBias));
        drawList->AddLine(spikeStart, spikeEnd, ToImColor(shadowColor, 0.92f),
                          outerThickness * (0.18f + 0.12f * spikeScale));
    }

    constexpr int kSparkCount = 12;
    for (int i = 0; i < kSparkCount; ++i) {
        float ratio = static_cast<float>(i) / static_cast<float>(kSparkCount);
        float angle = actionTime * 16.0f + ratio * DirectX::XM_2PI * 1.17f;
        float sparkLen =
            enemySlashSparkSpreadPx_ * (0.28f + 0.72f * (1.0f - ratio)) *
            (0.84f + 0.18f * pulse);
        float offset = enemySlashSparkSpreadPx_ * 0.20f * ratio;
        ImVec2 sparkStart(
            impact.x + dirX * offset + std::cosf(angle) * 8.0f,
            impact.y + dirY * offset + std::sinf(angle * 1.2f) * 8.0f);
        ImVec2 sparkEnd(
            sparkStart.x + std::cosf(angle) * sparkLen + perpX * sparkLen * 0.12f,
            sparkStart.y + std::sinf(angle) * sparkLen + perpY * sparkLen * 0.12f);

        drawList->AddLine(sparkStart, sparkEnd, ToImColor(hotColor, 0.82f - ratio * 0.3f),
                          1.2f + (1.0f - ratio) * 1.8f);
    }

    ImVec2 flashQuad[4] = {
        ImVec2(impact.x - perpX * (28.0f + phaseAlpha * 20.0f) - dirX * 4.0f,
               impact.y - perpY * (28.0f + phaseAlpha * 20.0f) - dirY * 4.0f),
        ImVec2(impact.x + dirX * (44.0f + phaseAlpha * 22.0f),
               impact.y + dirY * (44.0f + phaseAlpha * 22.0f)),
        ImVec2(impact.x + perpX * (16.0f + phaseAlpha * 10.0f) - dirX * 6.0f,
               impact.y + perpY * (16.0f + phaseAlpha * 10.0f) - dirY * 6.0f),
        ImVec2(impact.x - dirX * (18.0f + phaseAlpha * 8.0f),
               impact.y - dirY * (18.0f + phaseAlpha * 8.0f))};
    drawList->AddConvexPolyFilled(flashQuad, 4, ToImColor(coreColor, 0.78f));
    drawList->AddLine(ImVec2(impact.x - perpX * 42.0f, impact.y - perpY * 42.0f),
                      ImVec2(impact.x + perpX * 42.0f, impact.y + perpY * 42.0f),
                      ToImColor(shadowColor, 0.88f), 3.2f);

    drawList->AddCircleFilled(impact, 11.0f + phaseAlpha * 8.0f,
                              ToImColor(coreColor, 0.68f));
    drawList->AddCircleFilled(impact, 21.0f + phaseAlpha * 12.0f,
                              ToImColor(hotColor, 0.26f));
    drawList->AddCircleFilled(impact, 34.0f + phaseAlpha * 16.0f,
                              ToImColor(bloomColor, 0.14f));
#endif
}

void GameScene::UpdateEnemySlashEffects() {
    const ActionKind actionKind = enemy_.GetActionKind();
    const ActionStep actionStep = enemy_.GetActionStep();

    const bool isSlashActive =
        (actionKind == ActionKind::Smash || actionKind == ActionKind::Sweep) &&
        (actionStep == ActionStep::Active);

    if (isSlashActive && !enemySlashActiveLatched_) {
        DirectX::XMFLOAT3 slashStartWorld{};
        DirectX::XMFLOAT3 slashEndWorld{};
        float phaseAlpha = 0.0f;
        float actionTime = 0.0f;
        ActionKind worldActionKind = ActionKind::None;

        if (ComputeEnemySlashWorldEffect(slashStartWorld, slashEndWorld,
                                         phaseAlpha, actionTime,
                                         worldActionKind)) {
            DirectX::XMFLOAT3 effectPosition{
                (slashStartWorld.x + slashEndWorld.x) * 0.5f,
                (slashStartWorld.y + slashEndWorld.y) * 0.5f,
                (slashStartWorld.z + slashEndWorld.z) * 0.5f};
            RequestEffect(GpuSlashParticleSystem::EffectPreset::ChargeSpark,
                          effectPosition);
        }
    }

    enemySlashActiveLatched_ = isSlashActive;
    prevEnemyActionKind_ = actionKind;
    prevEnemyActionStep_ = actionStep;
}

void GameScene::RequestEffect(GpuSlashParticleSystem::EffectPreset preset,
                              const DirectX::XMFLOAT3 &position) {
    if (ctx_ == nullptr || ctx_->gpuSlashParticleSystem == nullptr) {
        return;
    }

    ctx_->gpuSlashParticleSystem->PlayParticle(preset, position);
}

void GameScene::UpdateEnemySwordTrail() {
    if (ctx_ == nullptr || ctx_->trailRenderer == nullptr) {
        return;
    }

    ctx_->trailRenderer->SetEnabled(enemySwordTrailEnabled_);
    ctx_->trailRenderer->BeginFrame(ctx_->deltaTime);

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

        ctx_->trailRenderer->EmitTrail(TrailRenderer::TrailPreset::SwordSlash,
                                       baseWorld, tipWorld, width);
    }

    ctx_->trailRenderer->EndFrame();
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
#ifdef IMGUI_DISABLED
    return;
#else
    if (enemy_.GetActionKind() != ActionKind::Warp) {
        return;
    }

    ImGuiContext *imguiCtx = ImGui::GetCurrentContext();
    if (imguiCtx == nullptr || imguiCtx->Viewports.Size <= 0) {
        return;
    }

    ActionStep warpStep = enemy_.GetActionStep();
    float stepIntensity = 0.0f;
    switch (warpStep) {
    case ActionStep::Start:
        stepIntensity = 0.72f;
        break;
    case ActionStep::Move:
        stepIntensity = 1.0f;
        break;
    case ActionStep::End:
        stepIntensity = 0.84f;
        break;
    default:
        return;
    }

    XMFLOAT2 targetScreenF{};
    bool hasTarget = ProjectWorldToScreen(enemy_.GetWarpTargetPos(), targetScreenF);
    ImVec2 targetScreen(targetScreenF.x, targetScreenF.y);

    XMFLOAT2 sourceScreenF{};
    bool hasSource = enemy_.HasWarpDeparturePos() &&
                     ProjectWorldToScreen(enemy_.GetWarpDeparturePos(),
                                          sourceScreenF);
    if (!hasSource && !hasTarget) {
        return;
    }
    ImVec2 sourceScreen(sourceScreenF.x, sourceScreenF.y);

    float time = enemy_.GetCurrentActionTimePublic();
    float baseRadius = warpDistortionRadiusPx_ * (0.85f + 0.30f * stepIntensity);
    float jitter = warpDistortionJitterPx_ * (0.80f + 0.40f * std::sinf(time * 20.0f));
    float alpha = warpDistortionAlpha_ + warpDistortionMoveAlphaBonus_ *
                                              (warpStep == ActionStep::Move ? 1.0f : 0.0f);

    ImGuiViewport *mainViewport = ImGui::GetMainViewport();
    if (mainViewport == nullptr) {
        return;
    }

    ImDrawList *drawList = ImGui::GetBackgroundDrawList(mainViewport);
    if (drawList == nullptr) {
        return;
    }
    ImU32 bright = IM_COL32(255, 48, 108,
                            static_cast<int>(255.0f * alpha));
    ImU32 soft = IM_COL32(120, 22, 70,
                          static_cast<int>(255.0f * (alpha * 0.82f)));
    ImU32 slash = IM_COL32(255, 215, 235,
                           static_cast<int>(255.0f * (alpha * 0.78f)));

    auto drawDistortionAt = [&](const ImVec2 &center, float radiusScale,
                                float rotationBias) {
        constexpr int kSegments = 28;
        ImVec2 points[kSegments + 1];
        for (int i = 0; i <= kSegments; ++i) {
            float ratio = static_cast<float>(i) / static_cast<float>(kSegments);
            float angle = ratio * DirectX::XM_2PI + time * 6.0f + rotationBias;
            float wave = std::sinf(angle * 3.0f + time * 17.0f) * jitter;
            float radius = baseRadius * radiusScale + wave;
            points[i] = ImVec2(center.x + std::cosf(angle) * radius,
                               center.y + std::sinf(angle) * radius);
        }

        drawList->AddPolyline(points, kSegments + 1, soft, true,
                              warpDistortionThicknessPx_);
        drawList->AddCircle(center, baseRadius * radiusScale * 0.62f, bright, 24,
                            warpDistortionThicknessPx_ * 0.7f);
        drawList->AddCircle(center, baseRadius * radiusScale * 0.82f, bright, 28,
                            warpDistortionThicknessPx_ * 0.42f);

        for (int i = 0; i < 8; ++i) {
            float ratio = static_cast<float>(i) / 8.0f;
            float angle = ratio * DirectX::XM_2PI + time * 8.5f + rotationBias;
            float inner = baseRadius * radiusScale * 0.42f;
            float outer = inner + warpDistortionLineLengthPx_ *
                                      (0.75f + 0.25f * std::sinf(time * 18.0f + i));
            ImVec2 a(center.x + std::cosf(angle) * inner,
                     center.y + std::sinf(angle) * inner);
            ImVec2 b(center.x + std::cosf(angle) * outer,
                     center.y + std::sinf(angle) * outer);
            drawList->AddLine(a, b, bright, 1.6f);
        }
    };

    if (warpStep == ActionStep::Start && hasSource) {
        ImVec2 sourceFoot = sourceScreen;
        sourceFoot.y += warpDistortionFootOffsetPx_;
        drawDistortionAt(sourceFoot, 0.88f, 0.0f);
    }

    if (warpStep == ActionStep::Move && hasSource && hasTarget) {
        ImVec2 mid((sourceScreen.x + targetScreen.x) * 0.5f,
                   (sourceScreen.y + targetScreen.y) * 0.5f +
                       warpDistortionFootOffsetPx_ * 0.78f);
        drawDistortionAt(mid, 0.72f, 0.6f);
        drawList->AddLine(sourceScreen, targetScreen, soft, 0.8f);
    }

    if (hasTarget) {
        ImVec2 arrivalCenter = targetScreen;
        arrivalCenter.y += warpDistortionFootOffsetPx_ -
                           warpDistortionPreviewOffsetPx_ *
                               (warpStep == ActionStep::Start ? 0.55f : 0.18f);
        drawDistortionAt(arrivalCenter,
                         warpStep == ActionStep::Move ? 1.12f : 1.0f, 1.2f);
    }

    float dirX = 0.0f;
    float dirY = -1.0f;
    if (hasSource && hasTarget) {
        dirX = targetScreen.x - sourceScreen.x;
        dirY = targetScreen.y - sourceScreen.y;
        float dirLen = std::sqrtf(dirX * dirX + dirY * dirY);
        if (dirLen > 0.0001f) {
            dirX /= dirLen;
            dirY /= dirLen;
        } else {
            dirX = 0.0f;
            dirY = -1.0f;
        }
    }

    float perpX = -dirY;
    float perpY = dirX;
    float slashLen =
        baseRadius * (warpStep == ActionStep::Move ? 0.92f : 1.10f);
    float branchLen = slashLen * 0.76f;
    float branchOffset = baseRadius * 0.28f;

    if (hasTarget) {
        ImVec2 arrivalCenter = targetScreen;
        arrivalCenter.y -= warpDistortionPreviewOffsetPx_ *
                           (warpStep == ActionStep::Start ? 0.55f : 0.18f);
        ImVec2 slashA(arrivalCenter.x - perpX * slashLen,
                      arrivalCenter.y - perpY * slashLen);
        ImVec2 slashB(arrivalCenter.x + perpX * slashLen,
                      arrivalCenter.y + perpY * slashLen);
        ImVec2 slashC(arrivalCenter.x - perpX * branchLen + dirX * branchOffset,
                      arrivalCenter.y - perpY * branchLen + dirY * branchOffset);
        ImVec2 slashD(arrivalCenter.x + perpX * branchLen + dirX * branchOffset,
                      arrivalCenter.y + perpY * branchLen + dirY * branchOffset);
        ImVec2 slashE(arrivalCenter.x - perpX * (branchLen * 0.58f) -
                          dirX * branchOffset * 0.72f,
                      arrivalCenter.y - perpY * (branchLen * 0.58f) -
                          dirY * branchOffset * 0.72f);
        ImVec2 slashF(arrivalCenter.x + perpX * (branchLen * 0.58f) -
                          dirX * branchOffset * 0.72f,
                      arrivalCenter.y + perpY * (branchLen * 0.58f) -
                          dirY * branchOffset * 0.72f);

        drawList->AddLine(slashA, slashB, slash,
                          warpDistortionThicknessPx_ * 0.82f);
        drawList->AddLine(slashC, slashD, bright,
                          warpDistortionThicknessPx_ * 0.55f);
        drawList->AddLine(slashE, slashF, soft,
                          warpDistortionThicknessPx_ * 0.42f);
    }
#endif
}

// void GameScene::UpdateCamera(Input *input) {
// #ifdef _DEBUG
//     if (input->IsKeyTrigger(DIK_F11)) {
//         if (currentCamera_ == &camera_) {
//             currentCamera_ = &debugCamera_;
//         } else {
//             currentCamera_ = &camera_;
//         }
//     }
//
//     if (currentCamera_ == &debugCamera_) {
//         debugCamera_.Update(*input, ctx_->deltaTime);
//         currentCamera_->UpdateMatrices();
//         return;
//     }
// #else
//     (void)input;
// #endif
// }

// void GameScene::UpdateBattleCamera() {
//     auto &playerTf = player_.GetTransform();
//     auto &enemyTf = enemy_.GetTransform();
//
//     XMFLOAT3 playerPos = playerTf.position;
//     XMFLOAT3 enemyPos = enemyTf.position;
//
//     XMVECTOR playerPosV = XMLoadFloat3(&playerPos);
//     XMVECTOR enemyPosV = XMLoadFloat3(&enemyPos);
//
//     XMVECTOR forward = XMVector3Normalize(enemyPosV - playerPosV);
//
//     XMVECTOR camPos = playerPosV - forward * kCameraDistance +
//                       XMVectorSet(0, kCameraHeight, 0, 0);
//
//     XMFLOAT3 cameraPos;
//     XMStoreFloat3(&cameraPos, camPos);
//
//     camera_.SetPosition(cameraPos);
//     camera_.LookAt({enemyPos.x, kCameraHeight, enemyPos.z});
// }

void GameScene::UpdateCamera(Input *input) {
#ifdef _DEBUG
    // 蛻・�E�譖ｿ縺・
    if (input->IsKeyTrigger(DIK_F11)) {
        currentCamera_ = &debugCamera_;
    }
    if (input->IsKeyTrigger(DIK_F10)) {
        currentCamera_ = &camera_;
    }
    if (input->IsKeyTrigger(DIK_F9)) {
        currentCamera_ = &tripodCamera_;
    }

    // DebugCamera
    if (currentCamera_ == &debugCamera_) {
        debugCamera_.Update(*input, ctx_->deltaTime);
        debugCamera_.UpdateMatrices();
        return;
    }

    // TripodCamera・亥�E�悟�E蝗ｺ螳夲�E�・
    if (currentCamera_ == &tripodCamera_) {
        tripodCamera_.SetPosition(tripodPos_);
        tripodCamera_.LookAt(tripodTarget_);
        tripodCamera_.UpdateMatrices();
        return;
    }
#endif

    // ===== 騾壼�E��E�繧�E�繝｡繝ｩ =====

    // 繝ｭ繝�Eけ繧�E�繝ｳ蛻・�E�譖ｿ縺・
    if (input->IsKeyTrigger(DIK_Q)) {
        isLockOn_ = !isLockOn_;
    }

    float yawInput = 0.0f;
    float pitchInput = 0.0f;

#ifdef _DEBUG
    if (input->IsKeyPress(DIK_LEFT)) {
        yawInput -= 1.0f;
    }
    if (input->IsKeyPress(DIK_RIGHT)) {
        yawInput += 1.0f;
    }
    if (input->IsKeyPress(DIK_UP)) {
        pitchInput += 1.0f;
    }
    if (input->IsKeyPress(DIK_DOWN)) {
        pitchInput -= 1.0f;
    }
#endif

    cameraYaw_ += yawInput * cameraLookSensitivity_;
    cameraPitch_ += pitchInput * cameraLookSensitivity_;

    if (cameraPitch_ < cameraPitchMin_) {
        cameraPitch_ = cameraPitchMin_;
    }
    if (cameraPitch_ > cameraPitchMax_) {
        cameraPitch_ = cameraPitchMax_;
    }

    // 箝�E譛驥崎ｦ・�E�壹�E�E��薙〒繧�E�繝｡繝ｩ遒ｺ螳・
    UpdateBattleCamera();

    // 箝�E譛驥崎ｦ・�E�夊｡悟�E譖ｴ譁E��
    camera_.UpdateMatrices();
}

void GameScene::UpdateBattleCamera() {
    const auto &playerTf = player_.GetTransform();
    const auto &enemyTf = enemy_.GetTransform();

    const DirectX::XMFLOAT3 &playerPos = playerTf.position;
    const DirectX::XMFLOAT3 &enemyPos = enemyTf.position;

    // 謨�E�陦悟虚迥�E�諷九ｒ蜿門�E�・
    const ActionKind enemyActionKind = enemy_.GetActionKind();
    const ActionStep enemyActionStep = enemy_.GetActionStep();

    const bool isEnemyWarpStart = (enemyActionKind == ActionKind::Warp &&
                                   enemyActionStep == ActionStep::Start);

    const bool isEnemyWarpMove = (enemyActionKind == ActionKind::Warp &&
                                  enemyActionStep == ActionStep::Move);

    const bool isEnemyWarpEnd = (enemyActionKind == ActionKind::Warp &&
                                 enemyActionStep == ActionStep::End);
    const bool isEnemyIntro = enemy_.IsIntroActive();
    const float enemyIntroRatio = enemy_.GetIntroRatio();
    const bool isEnemyPhaseTransition = enemy_.IsPhaseTransitionActive();
    const float enemyPhaseTransitionRatio = enemy_.GetPhaseTransitionRatio();

    // =========================
    // FOV繧�E�繝ｼ繧�E�繝�Eヨ豎ｺ螳・
    // =========================
    targetFovDeg_ = normalFovDeg_;

    if (isLockOn_) {
        targetFovDeg_ = lockOnFovDeg_;
    }

    if (isEnemyWarpStart || isEnemyWarpMove || isEnemyWarpEnd) {
        targetFovDeg_ = warpFovDeg_;
    }

    if (isEnemyIntro) {
        targetFovDeg_ = enemyIntroFovDeg_;
    }

    if (isEnemyPhaseTransition) {
        targetFovDeg_ = phaseTransitionFovDeg_;
    }

    float usedFovLerpSpeed = fovLerpSpeed_;
    if (isEnemyIntro) {
        usedFovLerpSpeed = enemyIntroFovLerpSpeed_;
    }
    if (isEnemyPhaseTransition) {
        usedFovLerpSpeed = phaseTransitionFovLerpSpeed_;
    }
    float fovAlpha = usedFovLerpSpeed * ctx_->deltaTime;
    if (fovAlpha > 1.0f) {
        fovAlpha = 1.0f;
    }

    currentFovDeg_ += (targetFovDeg_ - currentFovDeg_) * fovAlpha;
    camera_.SetPerspectiveFovDeg(currentFovDeg_);

    // =========================
    // 繝ｭ繝�Eけ繧�E�繝ｳ荳�E�縺�E�縺・yaw 陬懷勧
    // =========================
    if (isLockOn_ || isEnemyIntro) {
        DirectX::XMFLOAT3 assistTarget = enemyPos;

        float assistStrength = lockOnAssistStrength_;
        float assistMaxStep = lockOnAssistMaxStep_;

        if (isEnemyIntro) {
            assistStrength = lockOnAssistStrength_ * 1.55f;
            assistMaxStep = lockOnAssistMaxStep_ * 1.55f;
        } else if (isEnemyWarpStart) {
            assistStrength = warpStartAssistStrength_;
            assistMaxStep = warpStartAssistMaxStep_;
        } else if (isEnemyWarpMove) {
            // 繝ｯ繝ｼ繝礼�E��E�蜍穂ｸ�E�縺�E�辟｡送E�E↓謖ｯ繧雁屓縺輔�E縺・
            assistStrength = 0.0f;
            assistMaxStep = 0.0f;
        } else if (isEnemyWarpEnd) {
            assistStrength = warpEndAssistStrength_;
            assistMaxStep = warpEndAssistMaxStep_;
        } else if (isEnemyPhaseTransition) {
            assistStrength = lockOnAssistStrength_ * 1.35f;
            assistMaxStep = lockOnAssistMaxStep_ * 1.35f;
        }

        float dx = assistTarget.x - playerPos.x;
        float dz = assistTarget.z - playerPos.z;

        float targetYaw = std::atan2f(dx, dz);
        float diff = targetYaw - cameraYaw_;

        while (diff > 3.14159265f) {
            diff -= 6.28318530f;
        }
        while (diff < -3.14159265f) {
            diff += 6.28318530f;
        }

        float inputMagnitude = 0.0f;
#ifdef _DEBUG
        Input *input = ctx_->input;
        if (input->IsKeyPress(DIK_LEFT) || input->IsKeyPress(DIK_RIGHT)) {
            inputMagnitude = 1.0f;
        }
#endif

        float assistScale = 1.0f;
        if (inputMagnitude > 0.0f) {
            assistScale = lockOnInputReduce_;
        }

        float maxStep = assistMaxStep * assistScale * ctx_->deltaTime;
        float applied = diff * assistStrength * assistScale * ctx_->deltaTime;

        if (applied > maxStep) {
            applied = maxStep;
        } else if (applied < -maxStep) {
            applied = -maxStep;
        }

        cameraYaw_ += applied;
    }

    // =========================
    // yaw / pitch 縺九ｉ蝓�E�貁E�E�E��E�繧剁E��懊ａE
    // =========================
    float cosPitch = std::cosf(cameraPitch_);
    DirectX::XMFLOAT3 forward = {std::sinf(cameraYaw_) * cosPitch,
                                 std::sinf(cameraPitch_),
                                 std::cosf(cameraYaw_) * cosPitch};

    DirectX::XMFLOAT3 right = {std::cosf(cameraYaw_), 0.0f,
                               -std::sinf(cameraYaw_)};

    // =========================
    // 繧�E�繝｡繝ｩ蝓ｺ貁E��せ
    // =========================
    DirectX::XMFLOAT3 cameraTargetBase = {
        playerPos.x, playerPos.y + cameraLookHeight_, playerPos.z};

    DirectX::XMFLOAT3 cameraPos{};

    if (isLockOn_) {
        // ---------------------------------
        // 繝ｭ繝�Eけ繧�E�繝ｳ譎�E 謨�E�縺�E�縺�E�繝ｩ繧�E�繝ｳ蝓ｺ貁E��〒蜀・�E��E�霑ｽ蠕�E
        // ---------------------------------
        float toEnemyX = enemyPos.x - playerPos.x;
        float toEnemyZ = enemyPos.z - playerPos.z;
        float distXZ = std::sqrt(toEnemyX * toEnemyX + toEnemyZ * toEnemyZ);

        if (distXZ < 0.0001f) {
            distXZ = 1.0f;
        }

        float invLen = 1.0f / distXZ;
        float lineX = toEnemyX * invLen;
        float lineZ = toEnemyZ * invLen;

        // 謨�E�譁E��蜷代Λ繧�E�繝ｳ縺�E�蟁E��縺吶�E�蜿�E�繝吶け繝医΁E
        float orbitRightX = lineZ;
        float orbitRightZ = -lineX;

        // 謨�E�縺�E�縺�E�霍晞屬縺�E�蟁E���E�縺�E�縺大�E�後ｍ縺�E�蠑輔￥
        float pullT = 0.0f;
        {
            float minD = lockOnDistanceMin_;
            float maxD = lockOnDistanceMax_;
            float range = maxD - minD;
            if (range > 0.0001f) {
                pullT = (distXZ - minD) / range;
            }
            if (pullT < 0.0f) {
                pullT = 0.0f;
            }
            if (pullT > 1.0f) {
                pullT = 1.0f;
            }
        }

        float usedRadius = lockOnOrbitRadius_ + lockOnOrbitPullBackMax_ * pullT;
        if (isEnemyIntro) {
            usedRadius -= enemyIntroPushIn_ * enemyIntroRatio;
        }
        if (isEnemyPhaseTransition) {
            usedRadius -= phaseTransitionPushIn_ * enemyPhaseTransitionRatio;
        }

        // cameraYaw_ 縺�E�謨�E�譁E��蜷代Λ繧�E�繝ｳ縺�E�縺�E�蟾�E�縺�E�縲∝�E蠑ｧ荳翫・蟾�E�蜿�E�菴咲�E��E�繧呈ｱ�E�繧√ａE
        float lineYaw = std::atan2f(lineX, lineZ);
        float yawDiff = cameraYaw_ - lineYaw;

        while (yawDiff > 3.14159265f) {
            yawDiff -= 6.28318530f;
        }
        while (yawDiff < -3.14159265f) {
            yawDiff += 6.28318530f;
        }

        // 逵滓ｨ�E�縺�E�縺�E�蝗槭�E�縺吶℁E��九�E隕九▼繧峨�E�縺�E�縺�E�蛻�E�髯・
        const float maxOrbitAngle = 0.65f;
        if (yawDiff > maxOrbitAngle) {
            yawDiff = maxOrbitAngle;
        } else if (yawDiff < -maxOrbitAngle) {
            yawDiff = -maxOrbitAngle;
        }

        float sinA = std::sinf(yawDiff);
        float cosA = std::cosf(yawDiff);

        DirectX::XMFLOAT3 pairCenter = {
            playerPos.x * lockOnLookPlayerWeight_ +
                enemyPos.x * lockOnLookEnemyWeight_,
            (playerPos.y + cameraLookHeight_) * 0.55f +
                (enemyPos.y + 1.35f) * 0.45f,
            playerPos.z * lockOnLookPlayerWeight_ +
                enemyPos.z * lockOnLookEnemyWeight_};

        DirectX::XMFLOAT3 desiredCameraPos = {
            pairCenter.x - lineX * usedRadius * cosA +
                orbitRightX * usedRadius * sinA +
                orbitRightX * lockOnOrbitSideBias_,
            pairCenter.y + lockOnOrbitHeight_ + 0.20f * pullT,
            pairCenter.z - lineZ * usedRadius * cosA +
                orbitRightZ * usedRadius * sinA +
                orbitRightZ * lockOnOrbitSideBias_};

        float posAlpha = lockOnOrbitLerpSpeed_ * ctx_->deltaTime;
        if (posAlpha > 1.0f) {
            posAlpha = 1.0f;
        }

        lockOnOrbitCameraPos_.x +=
            (desiredCameraPos.x - lockOnOrbitCameraPos_.x) * posAlpha;
        lockOnOrbitCameraPos_.y +=
            (desiredCameraPos.y - lockOnOrbitCameraPos_.y) * posAlpha;
        lockOnOrbitCameraPos_.z +=
            (desiredCameraPos.z - lockOnOrbitCameraPos_.z) * posAlpha;

        cameraPos = lockOnOrbitCameraPos_;
    } else {
        // ---------------------------------
        // 騾壼�E��E�譎�E 閧�E�雜翫�E�荳我ｺ�E�遘ｰ
        // ---------------------------------
        float dx = enemyPos.x - playerPos.x;
        float dz = enemyPos.z - playerPos.z;
        float enemyDistanceXZ = std::sqrt(dx * dx + dz * dz);
        float dynamicDistance = cameraDistance_;
        if (enemyDistanceXZ > 5.0f) {
            dynamicDistance +=
                (std::min)(1.1f, (enemyDistanceXZ - 5.0f) * 0.18f);
        }

        cameraPos = {cameraTargetBase.x - forward.x * dynamicDistance +
                         right.x * cameraSideOffset_,
                     cameraTargetBase.y + cameraHeight_ -
                         forward.y * dynamicDistance,
                     cameraTargetBase.z - forward.z * dynamicDistance +
                         right.z * cameraSideOffset_};

        if (isEnemyIntro) {
            cameraPos.x += forward.x * enemyIntroPushIn_ * enemyIntroRatio;
            cameraPos.y += 0.18f * enemyIntroRatio;
            cameraPos.z += forward.z * enemyIntroPushIn_ * enemyIntroRatio;
        }
        if (isEnemyPhaseTransition) {
            cameraPos.x += forward.x * phaseTransitionPushIn_ *
                           enemyPhaseTransitionRatio;
            cameraPos.y += 0.12f * enemyPhaseTransitionRatio;
            cameraPos.z += forward.z * phaseTransitionPushIn_ *
                           enemyPhaseTransitionRatio;
        }

        // 髱槭Ο繝�Eけ譎ゅ・蜀・�E��E�逕ｨ迴�E�蝨�E�蛟､繧貞�E譛�E
        lockOnOrbitCameraPos_ = cameraPos;
    }

    // =========================
    // 豕ｨ隕也せ
    // =========================
    DirectX::XMFLOAT3 lookAt{};

    if (isLockOn_) {
        DirectX::XMFLOAT3 desiredLookAt = {
            playerPos.x * lockOnLookPlayerWeight_ +
                enemyPos.x * lockOnLookEnemyWeight_,
            (playerPos.y + cameraLookHeight_) * 0.52f +
                (enemyPos.y + 1.30f) * 0.48f,
            playerPos.z * lockOnLookPlayerWeight_ +
                enemyPos.z * lockOnLookEnemyWeight_};

        float lookAlpha = lockOnLookAtLerpSpeed_ * ctx_->deltaTime;
        if (lookAlpha > 1.0f) {
            lookAlpha = 1.0f;
        }

        lockOnLookAt_.x += (desiredLookAt.x - lockOnLookAt_.x) * lookAlpha;
        lockOnLookAt_.y += (desiredLookAt.y - lockOnLookAt_.y) * lookAlpha;
        lockOnLookAt_.z += (desiredLookAt.z - lockOnLookAt_.z) * lookAlpha;

        lookAt = lockOnLookAt_;
    } else {
        lookAt = {cameraTargetBase.x + forward.x * cameraLookAhead_,
                  cameraTargetBase.y + forward.y * cameraLookAhead_,
                  cameraTargetBase.z + forward.z * cameraLookAhead_};

        lockOnLookAt_ = lookAt;
    }

    if (isEnemyIntro) {
        DirectX::XMFLOAT3 introLookAt = {
            playerPos.x * (1.0f - enemyIntroLookAtEnemyWeight_) +
                enemyPos.x * enemyIntroLookAtEnemyWeight_,
            (playerPos.y + cameraLookHeight_) *
                    (1.0f - enemyIntroLookAtEnemyWeight_) +
                (enemyPos.y + enemyIntroLookAtHeight_) *
                    enemyIntroLookAtEnemyWeight_,
            playerPos.z * (1.0f - enemyIntroLookAtEnemyWeight_) +
                enemyPos.z * enemyIntroLookAtEnemyWeight_};

        float blend = enemyIntroRatio;
        lookAt.x += (introLookAt.x - lookAt.x) * blend;
        lookAt.y += (introLookAt.y - lookAt.y) * blend;
        lookAt.z += (introLookAt.z - lookAt.z) * blend;
    }

    if (isEnemyPhaseTransition) {
        DirectX::XMFLOAT3 transitionLookAt = {
            playerPos.x * (1.0f - phaseTransitionLookAtEnemyWeight_) +
                enemyPos.x * phaseTransitionLookAtEnemyWeight_,
            (playerPos.y + cameraLookHeight_) *
                    (1.0f - phaseTransitionLookAtEnemyWeight_) +
                (enemyPos.y + phaseTransitionLookAtHeight_) *
                    phaseTransitionLookAtEnemyWeight_,
            playerPos.z * (1.0f - phaseTransitionLookAtEnemyWeight_) +
                enemyPos.z * phaseTransitionLookAtEnemyWeight_};

        float blend = enemyPhaseTransitionRatio;
        lookAt.x += (transitionLookAt.x - lookAt.x) * blend;
        lookAt.y += (transitionLookAt.y - lookAt.y) * blend;
        lookAt.z += (transitionLookAt.z - lookAt.z) * blend;
    }

    if (isEnemyIntro) {
        float enemyYaw = enemy_.GetFacingYaw();
        float frontX = std::sinf(enemyYaw);
        float frontZ = std::cosf(enemyYaw);
        float rightX = std::cosf(enemyYaw);
        float rightZ = -std::sinf(enemyYaw);
        float introSide =
            (enemy_.GetIntroPhase() == IntroPhase::SpinSlash) ? 0.42f : 0.16f;
        float introDistance = 10.2f - 0.35f * enemyIntroRatio;

        cameraPos = {
            enemyPos.x + frontX * introDistance + rightX * introSide,
            enemyPos.y + 2.12f + 0.10f * enemyIntroRatio,
            enemyPos.z + frontZ * introDistance + rightZ * introSide};

        lookAt = {enemyPos.x, enemyPos.y + enemyIntroLookAtHeight_ + 0.18f,
                  enemyPos.z};
        cameraYaw_ = std::atan2f(enemyPos.x - cameraPos.x, enemyPos.z - cameraPos.z);
    }

    camera_.SetPosition(cameraPos);
    camera_.LookAt(lookAt);
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
    if (ctx_ == nullptr || ctx_->renderer.postEffectRenderer == nullptr ||
        currentCamera_ == nullptr) {
        return;
    }
    ctx_->renderer.postEffectRenderer->UpdateScreenEffects(
        ctx_->deltaTime, *currentCamera_, ctx_->frame.width, ctx_->frame.height);
}

