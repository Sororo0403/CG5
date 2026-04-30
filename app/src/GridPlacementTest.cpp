#include "GridPlacementTest.h"
#include "DirectXCommon.h"
#include "DebugUIRegistry.h"
#include "EngineRuntime.h"
#include "Inspector.h"
#include "Input.h"
#include "Material.h"
#include "Model.h"
#include "ModelManager.h"
#include "ModelRenderer.h"
#include "TextureManager.h"
#ifdef _DEBUG
#include "imgui.h"
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>

using namespace DirectX;

namespace {

constexpr float kFloorHeight = 0.0f;
constexpr float kMarkerHeight = 0.08f;

const char *GetPlacementKindName(PlacementObjectKind kind) {
    switch (kind) {
    case PlacementObjectKind::Floor:
        return "Floor";
    case PlacementObjectKind::Wall:
        return "Wall";
    case PlacementObjectKind::PlayerStart:
        return "PlayerStart";
    case PlacementObjectKind::GoalMarker:
        return "GoalMarker";
    default:
        return "Unknown";
    }
}

bool IsPlacementObjectKind(const std::string &kindName,
                           PlacementObjectKind *kind) {
    if (kindName == "Floor") {
        *kind = PlacementObjectKind::Floor;
        return true;
    }
    if (kindName == "Wall") {
        *kind = PlacementObjectKind::Wall;
        return true;
    }
    if (kindName == "PlayerStart" || kindName == "Player Start") {
        *kind = PlacementObjectKind::PlayerStart;
        return true;
    }
    if (kindName == "GoalMarker" || kindName == "Goal Marker" ||
        kindName == "EnemyMarker" || kindName == "Enemy Marker") {
        *kind = PlacementObjectKind::GoalMarker;
        return true;
    }
    return false;
}

std::string GetPlacementKindDisplayName(PlacementObjectKind kind) {
    switch (kind) {
    case PlacementObjectKind::Floor:
        return "Floor";
    case PlacementObjectKind::Wall:
        return "Wall";
    case PlacementObjectKind::PlayerStart:
        return "Player Start";
    case PlacementObjectKind::GoalMarker:
        return "GoalMarker";
    default:
        return "Unknown";
    }
}

char GetDefaultMapCode(PlacementObjectKind kind) {
    switch (kind) {
    case PlacementObjectKind::Floor:
        return '1';
    case PlacementObjectKind::Wall:
        return '2';
    case PlacementObjectKind::PlayerStart:
        return '3';
    case PlacementObjectKind::GoalMarker:
        return '4';
    default:
        return '0';
    }
}

template <typename T>
bool ReadFloatArray(const nlohmann::json &json, T &value) {
    if (!json.is_array() || json.size() != 3) {
        return false;
    }
    value.x = json[0].get<float>();
    value.y = json[1].get<float>();
    value.z = json[2].get<float>();
    return true;
}

bool ReadRotationArray(const nlohmann::json &json, XMFLOAT4 &value) {
    if (!json.is_array() || json.size() != 4) {
        return false;
    }
    value.x = json[0].get<float>();
    value.y = json[1].get<float>();
    value.z = json[2].get<float>();
    value.w = json[3].get<float>();
    return true;
}

void NormalizeQuaternion(XMFLOAT4 &rotation) {
    const float lengthSq = rotation.x * rotation.x + rotation.y * rotation.y +
                           rotation.z * rotation.z + rotation.w * rotation.w;
    if (lengthSq <= 0.000001f) {
        rotation = {0.0f, 0.0f, 0.0f, 1.0f};
        return;
    }

    const float invLength = 1.0f / std::sqrt(lengthSq);
    rotation.x *= invLength;
    rotation.y *= invLength;
    rotation.z *= invLength;
    rotation.w *= invLength;
}

void WarnDebug(const char *message) {
#ifdef _DEBUG
    std::fprintf(stderr, "%s\n", message);
#else
    (void)message;
#endif
}

bool IsImGuiCapturingInput() {
#ifdef _DEBUG
    const ImGuiIO &io = ImGui::GetIO();
    return io.WantCaptureKeyboard || io.WantCaptureMouse;
#else
    return false;
#endif
}

Material MakeMaterial(const XMFLOAT4 &color, float reflection = 0.08f) {
    Material material{};
    material.color = color;
    material.enableTexture = 1;
    material.reflectionStrength = reflection;
    material.reflectionFresnelStrength = 0.04f;
    material.reflectionRoughness = 0.55f;
    material.enableDissolve = 0;
    return material;
}

} // namespace

EditableObjectDesc PlacementObject::GetEditorDesc() const {
    EditableObjectDesc desc{};
    desc.name = name;
    desc.type = GetPlacementKindName(kind);
    if (kind == PlacementObjectKind::Wall) {
        desc.collider = "blocked grid tile";
    } else if (kind == PlacementObjectKind::Floor ||
               kind == PlacementObjectKind::PlayerStart ||
               kind == PlacementObjectKind::GoalMarker) {
        desc.collider = "walkable grid tile";
    }
    return desc;
}

void PlacementObject::SetEditorName(const std::string &newName) {
    name = newName;
}

EditableTransform PlacementObject::GetEditorTransform() const {
    EditableTransform editable{};
    editable.position = transform.position;
    editable.rotation = transform.rotation;
    editable.scale = transform.scale;
    return editable;
}

void PlacementObject::SetEditorTransform(const EditableTransform &editable) {
    transform.position = editable.position;
    transform.rotation = editable.rotation;
    transform.scale = editable.scale;
}

void GridPlacementTest::Initialize(const SceneContext &ctx) {
    CreateModels(ctx);
    map_.LoadFromJson(stagePath_);
    placementTileSize_ = map_.GetTileSize();
    bool hasObjects = false;
    if (!LoadObjectsFromJson(stagePath_, &hasObjects) || !hasObjects) {
        BuildObjects();
    }
    RecomputePlayerSpawnFromObjects();
    ResetPlayerToSpawn();
    lastAppliedTileSize_ = placementTileSize_;
    lastAppliedFloorScale_ = floorScale_;
    lastAppliedWallScale_ = wallScale_;
    lastAppliedWallHeight_ = wallHeight_;
    lastAppliedMarkerScale_ = markerScale_;
    RegisterDebugUI();
}

void GridPlacementTest::CreateModels(const SceneContext &ctx) {
    if (!ctx.assets.model || !ctx.assets.texture || !ctx.core.dxCommon) {
        return;
    }

    auto uploadContext = ctx.core.dxCommon->BeginUploadContext();
    const uint32_t whiteTextureId =
        ctx.assets.texture->Load(uploadContext, L"resources/model/white.png");
    playerModelId_ =
        ctx.assets.model->Load(uploadContext, L"resources/model/sneakWalk.gltf");
    uploadContext.Finish();
    ctx.assets.texture->ReleaseUploadBuffers();

    floorModelId_ = ctx.assets.model->CreatePlane(
        whiteTextureId, MakeMaterial({0.28f, 0.56f, 0.48f, 1.0f}, 0.05f));
    wallModelId_ = ctx.assets.model->CreateCylinder(
        whiteTextureId, MakeMaterial({0.38f, 0.42f, 0.48f, 1.0f}, 0.12f), 4,
        0.55f, 0.55f, 1.0f);
    markerModelId_ = ctx.assets.model->CreateCylinder(
        whiteTextureId, MakeMaterial({0.95f, 0.30f, 0.18f, 1.0f}, 0.18f), 24,
        0.35f, 0.35f, 1.0f);
}

void GridPlacementTest::BuildObjects() {
    objects_.clear();
    nextObjectId_ = 1;

    const XMFLOAT4 floorRotation = MakeQuaternion(-XM_PIDIV2, 0.0f, 0.0f);
    const float tileSize = map_.GetTileSize();

    for (int y = 0; y < map_.GetHeight(); ++y) {
        for (int x = 0; x < map_.GetWidth(); ++x) {
            const char tile = map_.GetTile(x, y);
            if (tile == '0') {
                continue;
            }

            PlacementObject floor =
                CreatePlacementObject(PlacementObjectKind::Floor, x, y);
            floor.mapCode = tile;
            floor.transform.rotation = floorRotation;
            floor.transform.scale = {tileSize * floorScale_,
                                     tileSize * floorScale_, 1.0f};
            objects_.push_back(floor);

            if (tile == '2') {
                PlacementObject wall =
                    CreatePlacementObject(PlacementObjectKind::Wall, x, y);
                objects_.push_back(wall);
            } else if (tile == '3') {
                PlacementObject player =
                    CreatePlacementObject(PlacementObjectKind::PlayerStart, x,
                                          y);
                objects_.push_back(player);
            } else if (tile == '4') {
                PlacementObject marker =
                    CreatePlacementObject(PlacementObjectKind::GoalMarker, x,
                                          y);
                objects_.push_back(marker);
            }
        }
    }

    ClampSelectedIndex();
    RecomputePlayerSpawnFromObjects();
}

void GridPlacementTest::Update(const SceneContext &ctx, Camera &camera) {
    if (ctx.assets.model) {
        ctx.assets.model->UpdateAnimation(playerModelId_, ctx.frame.deltaTime);
    }
    if (EngineRuntime::GetInstance().IsEditorMode()) {
        UpdateTuningMode(ctx, camera);
    } else {
        UpdatePlayMode(ctx, camera);
    }
    UpdateObjectTuning();
    RegisterInspectorObjects(camera);
}

void GridPlacementTest::UpdateCamera(const SceneContext &ctx, Camera &camera) {
    const Input *input = ctx.core.input;
    if (!input) {
        return;
    }
    if (IsImGuiCapturingInput()) {
        return;
    }

    const float dt = ctx.frame.deltaTime;

    if (input->IsKeyPress(DIK_A)) {
        cameraTarget_.x -= freeCameraMoveSpeed_ * dt;
    }
    if (input->IsKeyPress(DIK_D)) {
        cameraTarget_.x += freeCameraMoveSpeed_ * dt;
    }
    if (input->IsKeyPress(DIK_W)) {
        cameraTarget_.z += freeCameraMoveSpeed_ * dt;
    }
    if (input->IsKeyPress(DIK_S)) {
        cameraTarget_.z -= freeCameraMoveSpeed_ * dt;
    }
    if (input->IsKeyPress(DIK_Q)) {
        cameraDistance_ += freeCameraZoomSpeed_ * dt;
    }
    if (input->IsKeyPress(DIK_E)) {
        cameraDistance_ -= freeCameraZoomSpeed_ * dt;
    }
    if (input->IsKeyPress(DIK_LEFT)) {
        cameraYaw_ -= freeCameraRotateSpeed_ * dt;
    }
    if (input->IsKeyPress(DIK_RIGHT)) {
        cameraYaw_ += freeCameraRotateSpeed_ * dt;
    }
    if (input->IsKeyPress(DIK_UP)) {
        cameraHeight_ += freeCameraMoveSpeed_ * dt;
    }
    if (input->IsKeyPress(DIK_DOWN)) {
        cameraHeight_ -= freeCameraMoveSpeed_ * dt;
    }
    if (input->IsKeyTrigger(DIK_TAB)) {
        SelectObject(input->IsKeyPress(DIK_LSHIFT) ? -1 : 1);
    }

    cameraDistance_ = (std::clamp)(cameraDistance_, 3.0f, 22.0f);
    cameraHeight_ = (std::clamp)(cameraHeight_, 2.0f, 16.0f);

    const float sinYaw = std::sin(cameraYaw_);
    const float cosYaw = std::cos(cameraYaw_);
    const XMFLOAT3 cameraPosition = {
        cameraTarget_.x + sinYaw * cameraDistance_, cameraHeight_,
        cameraTarget_.z - cosYaw * cameraDistance_};
    camera.SetPosition(cameraPosition);
    camera.LookAt(cameraTarget_);
    camera.UpdateMatrices();
}

void GridPlacementTest::UpdateTuningMode(const SceneContext &ctx,
                                         Camera &camera) {
    UpdatePlacementInput(ctx);
    UpdateCamera(ctx, camera);
}

void GridPlacementTest::UpdatePlayMode(const SceneContext &ctx, Camera &camera) {
    const Input *input = ctx.core.input;
    if (!input) {
        return;
    }
    if (gameplayPaused_) {
        return;
    }

    const float dt = ctx.frame.deltaTime;
    float moveX = 0.0f;
    float moveZ = 0.0f;
    if (input->IsKeyPress(DIK_A)) {
        moveX -= 1.0f;
    }
    if (input->IsKeyPress(DIK_D)) {
        moveX += 1.0f;
    }
    if (input->IsKeyPress(DIK_W)) {
        moveZ += 1.0f;
    }
    if (input->IsKeyPress(DIK_S)) {
        moveZ -= 1.0f;
    }

    const float length = std::sqrt(moveX * moveX + moveZ * moveZ);
    if (length > 0.0f) {
        moveX /= length;
        moveZ /= length;
    }

    const float nextX = playerTransform_.position.x + moveX * playerMoveSpeed_ * dt;
    const float nextZ = playerTransform_.position.z + moveZ * playerMoveSpeed_ * dt;
    if (!IsBlocked(nextX, playerTransform_.position.z)) {
        playerTransform_.position.x = nextX;
    }
    if (!IsBlocked(playerTransform_.position.x, nextZ)) {
        playerTransform_.position.z = nextZ;
    }

    if (input->IsKeyTrigger(DIK_SPACE) && playerOnGround_) {
        playerVelocityY_ = playerJumpStrength_;
        playerOnGround_ = false;
    }
    playerVelocityY_ -= 14.0f * dt;
    playerTransform_.position.y += playerVelocityY_ * dt;
    if (playerTransform_.position.y <= playerSpawn_.y) {
        playerTransform_.position.y = playerSpawn_.y;
        playerVelocityY_ = 0.0f;
        playerOnGround_ = true;
    }
    if (input->IsKeyTrigger(DIK_R)) {
        ResetPlayerToSpawn();
    }

    const float follow = (std::clamp)(cameraFollowSpeed_ * dt, 0.0f, 1.0f);
    cameraTarget_.x += (playerTransform_.position.x - cameraTarget_.x) * follow;
    cameraTarget_.y += (playerTransform_.position.y - cameraTarget_.y) * follow;
    cameraTarget_.z += (playerTransform_.position.z - cameraTarget_.z) * follow;

    const float sinYaw = std::sin(cameraYaw_);
    const float cosYaw = std::cos(cameraYaw_);
    const XMFLOAT3 cameraPosition = {
        cameraTarget_.x + sinYaw * cameraDistance_, cameraTarget_.y + cameraHeight_,
        cameraTarget_.z - cosYaw * cameraDistance_};
    camera.SetPosition(cameraPosition);
    camera.LookAt(cameraTarget_);
    camera.UpdateMatrices();
}

void GridPlacementTest::UpdatePlacementInput(const SceneContext &ctx) {
    const Input *input = ctx.core.input;
    if (!input) {
        return;
    }
    if (IsImGuiCapturingInput()) {
        return;
    }

    if (input->IsKeyTrigger(DIK_J)) {
        --placementCursorX_;
    }
    if (input->IsKeyTrigger(DIK_L)) {
        ++placementCursorX_;
    }
    if (input->IsKeyTrigger(DIK_I)) {
        --placementCursorY_;
    }
    if (input->IsKeyTrigger(DIK_K)) {
        ++placementCursorY_;
    }
    placementCursorX_ = (std::clamp)(placementCursorX_, 0, map_.GetWidth() - 1);
    placementCursorY_ = (std::clamp)(placementCursorY_, 0, map_.GetHeight() - 1);

    if (input->IsKeyTrigger(DIK_RETURN)) {
        map_.SetTile(placementCursorX_, placementCursorY_,
                     GetBrushTile(placementBrush_));
        BuildObjects();
    }
    if (input->IsKeyTrigger(DIK_BACK)) {
        map_.SetTile(placementCursorX_, placementCursorY_, '0');
        BuildObjects();
    }
    if (input->IsKeyTrigger(DIK_F5)) {
        SaveScene(nullptr);
    }
    if (input->IsKeyTrigger(DIK_F9)) {
        LoadScene(nullptr);
    }
}

void GridPlacementTest::UpdateObjectTuning() {
    placementTileSize_ = (std::max)(0.25f, placementTileSize_);
    map_.SetTileSize(placementTileSize_);
    if (placementTileSize_ == lastAppliedTileSize_ &&
        floorScale_ == lastAppliedFloorScale_ &&
        wallScale_ == lastAppliedWallScale_ &&
        wallHeight_ == lastAppliedWallHeight_ &&
        markerScale_ == lastAppliedMarkerScale_) {
        return;
    }

    const float tileSize = map_.GetTileSize();

    for (PlacementObject &object : objects_) {
        if (object.kind == PlacementObjectKind::Floor) {
            object.transform.position =
                map_.GetCellCenter(object.gridX, object.gridY, kFloorHeight);
            object.transform.scale = {tileSize * floorScale_,
                                      tileSize * floorScale_, 1.0f};
        } else if (object.kind == PlacementObjectKind::Wall) {
            object.transform.position = map_.GetCellCenter(object.gridX,
                                                           object.gridY, 0.05f);
            object.transform.scale = {tileSize * wallScale_,
                                      tileSize * wallHeight_,
                                      tileSize * wallScale_};
        } else if (object.kind == PlacementObjectKind::GoalMarker) {
            object.transform.position =
                map_.GetCellCenter(object.gridX, object.gridY, kMarkerHeight);
            object.transform.scale = {tileSize * markerScale_,
                                      tileSize * (markerScale_ * 2.4f),
                                      tileSize * markerScale_};
        }
    }

    lastAppliedTileSize_ = placementTileSize_;
    lastAppliedFloorScale_ = floorScale_;
    lastAppliedWallScale_ = wallScale_;
    lastAppliedWallHeight_ = wallHeight_;
    lastAppliedMarkerScale_ = markerScale_;
}

void GridPlacementTest::DrawGBuffer(const SceneContext &ctx,
                                    const Camera &camera) {
    DrawObjects(ctx.renderer.model, ctx, camera, true, UINT32_MAX);
}

void GridPlacementTest::DrawForward(const SceneContext &ctx,
                                    const Camera &camera,
                                    uint32_t environmentTextureId) {
    DrawObjects(ctx.renderer.model, ctx, camera, false, environmentTextureId);
}

void GridPlacementTest::DrawObjects(ModelRenderer *renderer,
                                    const SceneContext &ctx,
                                    const Camera &camera, bool gBuffer,
                                    uint32_t environmentTextureId) {
    if (!renderer || !ctx.assets.model) {
        return;
    }

    renderer->PreDraw();
    if (ctx.renderer.light) {
        renderer->SetSceneLighting(ctx.renderer.light->GetSceneLighting());
    }

    for (const PlacementObject &object : objects_) {
        const Model *model = ctx.assets.model->GetModel(object.modelId);
        if (!model) {
            continue;
        }

        if (gBuffer) {
            renderer->DrawGBuffer(*model, object.transform, camera);
        } else {
            renderer->Draw(*model, object.transform, camera,
                           environmentTextureId);
        }
    }
    DrawPlayer(renderer, ctx, camera, gBuffer, environmentTextureId);

    renderer->PostDraw();
}

void GridPlacementTest::DrawPlayer(ModelRenderer *renderer,
                                   const SceneContext &ctx,
                                   const Camera &camera, bool gBuffer,
                                   uint32_t environmentTextureId) {
    if (!renderer || !ctx.assets.model || playerModelId_ == 0) {
        return;
    }

    const Model *model = ctx.assets.model->GetModel(playerModelId_);
    if (!model) {
        return;
    }

    if (gBuffer) {
        renderer->DrawGBuffer(*model, playerTransform_, camera);
    } else {
        renderer->Draw(*model, playerTransform_, camera, environmentTextureId);
    }
}

void GridPlacementTest::RegisterDebugUI() {
#ifdef _DEBUG
    DebugUIRegistry &registry = DebugUIRegistry::GetInstance();
    registry.RegisterFloat("Player", "Move Speed", &playerMoveSpeed_, 0.5f,
                           12.0f, true);
    registry.RegisterFloat("Player", "Jump Strength", &playerJumpStrength_,
                           1.0f, 14.0f, true);
    registry.RegisterFloat("Camera", "Distance", &cameraDistance_, 3.0f,
                           22.0f, true);
    registry.RegisterFloat("Camera", "Height", &cameraHeight_, 2.0f, 16.0f,
                           true);
    registry.RegisterFloat("Camera", "Follow Speed", &cameraFollowSpeed_,
                           1.0f, 20.0f, true);
    registry.RegisterFloat("Camera", "Free Move Speed", &freeCameraMoveSpeed_,
                           1.0f, 12.0f, true);
    registry.RegisterFloat("Placement", "Tile Size", &placementTileSize_, 0.5f,
                           3.0f, true);
    registry.RegisterFloat("Placement", "Floor Scale", &floorScale_, 0.5f,
                           1.5f, true);
    registry.RegisterFloat("Placement", "Wall Width", &wallScale_, 0.2f,
                           1.5f, true);
    registry.RegisterFloat("Placement", "Wall Height", &wallHeight_, 0.2f,
                           4.0f, true);
    registry.RegisterFloat("Placement", "Marker Scale", &markerScale_, 0.2f,
                           1.5f, true);
    registry.RegisterInt("Placement", "Cursor X", &placementCursorX_, 0,
                         map_.GetWidth() - 1, false);
    registry.RegisterInt("Placement", "Cursor Y", &placementCursorY_, 0,
                         map_.GetHeight() - 1, false);
    registry.RegisterCombo("Placement", "Brush", &placementBrush_,
                           {"Floor", "Wall", "Player Start", "GoalMarker"},
                           false);
    registry.RegisterReadonlyInt("Placement", "Object Count", [this]() {
        return static_cast<int>(objects_.size());
    });
    registry.RegisterReadonlyInt("Placement", "Stage Saves",
                                 [this]() { return saveCount_; });
    registry.RegisterReadonlyInt("Placement", "Stage Loads",
                                 [this]() { return loadCount_; });
#endif // _DEBUG
}

void GridPlacementTest::RegisterInspectorObjects(const Camera &camera) {
#ifdef _DEBUG
    Inspector &inspector = Inspector::GetInstance();
    inspector.RegisterCameraInfo("Gameplay Camera", &camera);
    inspector.RegisterTransform("Player", &playerTransform_);
    if (!objects_.empty()) {
        selectedIndex_ =
            (std::clamp)(selectedIndex_, 0, static_cast<int>(objects_.size()) - 1);
        PlacementObject &selected = objects_[static_cast<size_t>(selectedIndex_)];
        inspector.RegisterTransform("Selected Placement", &selected.transform);
    }
#else
    (void)camera;
#endif // _DEBUG
}

size_t GridPlacementTest::GetEditableObjectCount() const {
    return objects_.size();
}

IEditableObject *GridPlacementTest::GetEditableObject(size_t index) {
    if (index >= objects_.size()) {
        return nullptr;
    }
    return &objects_[index];
}

const IEditableObject *GridPlacementTest::GetEditableObject(size_t index) const {
    if (index >= objects_.size()) {
        return nullptr;
    }
    return &objects_[index];
}

int GridPlacementTest::GetSelectedEditableObjectIndex() const {
    return selectedIndex_;
}

void GridPlacementTest::SetSelectedEditableObjectIndex(int index) {
    if (objects_.empty()) {
        selectedIndex_ = 0;
        return;
    }
    selectedIndex_ = (std::clamp)(index, 0, static_cast<int>(objects_.size()) - 1);
}

bool GridPlacementTest::SaveScene(std::string *message) {
    if (message) {
        *message = stagePath_;
    }

    if (SaveSceneToJson(stagePath_)) {
        ++saveCount_;
        return true;
    }
    return false;
}

bool GridPlacementTest::LoadScene(std::string *message) {
    if (message) {
        *message = stagePath_;
    }

    if (!map_.LoadFromJson(stagePath_)) {
        return false;
    }

    ++loadCount_;
    placementTileSize_ = map_.GetTileSize();
    bool hasObjects = false;
    if (!LoadObjectsFromJson(stagePath_, &hasObjects)) {
        return false;
    }
    if (!hasObjects) {
        BuildObjects();
    }
    RecomputePlayerSpawnFromObjects();
    ResetPlayerToSpawn();
    lastAppliedTileSize_ = placementTileSize_;
    lastAppliedFloorScale_ = floorScale_;
    lastAppliedWallScale_ = wallScale_;
    lastAppliedWallHeight_ = wallHeight_;
    lastAppliedMarkerScale_ = markerScale_;
    return true;
}

bool GridPlacementTest::CanEditObjects() const {
    return EngineRuntime::GetInstance().IsEditorMode();
}

bool GridPlacementTest::AddEditableObject(const std::string &type,
                                          std::string *message) {
    if (!CanEditObjects()) {
        if (message) {
            *message = "Object editing is disabled in Gameplay Mode";
        }
        return false;
    }

    PlacementObjectKind kind = PlacementObjectKind::Floor;
    if (!IsPlacementObjectKind(type, &kind)) {
        if (message) {
            *message = "Unknown object type: " + type;
        }
        return false;
    }

    if (kind == PlacementObjectKind::PlayerStart) {
        RemoveExistingPlayerStarts();
    }
    if (kind != PlacementObjectKind::Floor) {
        EnsureFloorObjectAtCell(placementCursorX_, placementCursorY_);
    }

    PlacementObject object =
        CreatePlacementObject(kind, placementCursorX_, placementCursorY_);
    objects_.push_back(object);
    selectedIndex_ = static_cast<int>(objects_.size()) - 1;
    SyncMapCellFromObjects(object.gridX, object.gridY);
    RecomputePlayerSpawnFromObjects();

    if (message) {
        *message = "Added " + GetPlacementKindDisplayName(kind);
    }
    return true;
}

bool GridPlacementTest::DeleteSelectedEditableObject(std::string *message) {
    if (!CanEditObjects()) {
        if (message) {
            *message = "Object editing is disabled in Gameplay Mode";
        }
        return false;
    }
    if (selectedIndex_ < 0 ||
        selectedIndex_ >= static_cast<int>(objects_.size())) {
        if (message) {
            *message = "No object selected";
        }
        return false;
    }

    const PlacementObject removed =
        objects_[static_cast<size_t>(selectedIndex_)];
    objects_.erase(objects_.begin() + selectedIndex_);
    ClampSelectedIndex();
    SyncMapCellFromObjects(removed.gridX, removed.gridY);
    RecomputePlayerSpawnFromObjects();

    if (message) {
        *message = "Deleted " + removed.name;
    }
    return true;
}

bool GridPlacementTest::DuplicateSelectedEditableObject(std::string *message) {
    if (!CanEditObjects()) {
        if (message) {
            *message = "Object editing is disabled in Gameplay Mode";
        }
        return false;
    }
    if (selectedIndex_ < 0 ||
        selectedIndex_ >= static_cast<int>(objects_.size())) {
        if (message) {
            *message = "No object selected";
        }
        return false;
    }

    const PlacementObject source =
        objects_[static_cast<size_t>(selectedIndex_)];
    if (source.kind == PlacementObjectKind::PlayerStart) {
        RemoveExistingPlayerStarts();
    }
    if (source.kind != PlacementObjectKind::Floor) {
        EnsureFloorObjectAtCell(placementCursorX_, placementCursorY_);
    }

    PlacementObject object = source;
    object.id = AllocateObjectId();
    object.name = source.name + " Copy";
    object.gridX = placementCursorX_;
    object.gridY = placementCursorY_;
    object.mapCode = GetDefaultMapCode(object.kind);

    const XMFLOAT3 cellCenter =
        map_.GetCellCenter(object.gridX, object.gridY,
                           object.transform.position.y);
    object.transform.position.x = cellCenter.x;
    object.transform.position.z = cellCenter.z;
    AssignRuntimeFields(object);

    objects_.push_back(object);
    selectedIndex_ = static_cast<int>(objects_.size()) - 1;
    SyncMapCellFromObjects(object.gridX, object.gridY);
    RecomputePlayerSpawnFromObjects();

    if (message) {
        *message = "Duplicated " + source.name;
    }
    return true;
}

void GridPlacementTest::OnEnterEditorMode() {
    EnterEditorMode();
}

void GridPlacementTest::OnEnterGameplayMode() {
    EnterGameplayMode();
}

void GridPlacementTest::SetGameplayPaused(bool paused) {
    PauseGameplay(paused);
}

bool GridPlacementTest::IsGameplayPaused() const {
    return gameplayPaused_;
}

void GridPlacementTest::ResetGameplay() {
    RecomputePlayerSpawnFromObjects();
    ResetPlayerToSpawn();
}

void GridPlacementTest::EnterEditorMode() {
    gameplayPaused_ = false;
    cameraTarget_ = editorCameraTarget_;
    cameraYaw_ = editorCameraYaw_;
    cameraDistance_ = editorCameraDistance_;
    cameraHeight_ = editorCameraHeight_;
}

void GridPlacementTest::EnterGameplayMode() {
    editorCameraTarget_ = cameraTarget_;
    editorCameraYaw_ = cameraYaw_;
    editorCameraDistance_ = cameraDistance_;
    editorCameraHeight_ = cameraHeight_;
    gameplayPaused_ = false;
    RecomputePlayerSpawnFromObjects();
    ResetPlayerToSpawn();
}

void GridPlacementTest::PauseGameplay(bool paused) {
    gameplayPaused_ = paused;
}

void GridPlacementTest::OnEditableObjectChanged(size_t index) {
    if (index >= objects_.size()) {
        return;
    }

    PlacementObject *object = &objects_[index];
    if (object->kind == PlacementObjectKind::PlayerStart) {
        RecomputePlayerSpawnFromObjects();
    }
}

void GridPlacementTest::RecomputePlayerSpawnFromObjects() {
    bool foundPlayerStart = false;
    int playerStartCount = 0;
    for (const PlacementObject &object : objects_) {
        if (object.kind != PlacementObjectKind::PlayerStart) {
            continue;
        }

        ++playerStartCount;
        if (!foundPlayerStart) {
            playerSpawn_ = object.transform.position;
            foundPlayerStart = true;
        }
    }

    if (!foundPlayerStart) {
        playerSpawn_ = {0.0f, 0.0f, 0.0f};
    }
    if (playerStartCount > 1) {
        WarnDebug("GridPlacementTest: multiple PlayerStart objects found; using the first one.");
    }
}

bool GridPlacementTest::LoadObjectsFromJson(const std::string &path,
                                            bool *hasObjects) {
    if (hasObjects) {
        *hasObjects = false;
    }

    std::ifstream file(path);
    if (!file) {
        return false;
    }

    nlohmann::json root;
    try {
        file >> root;
    } catch (const nlohmann::json::exception &) {
        return false;
    }

    if (!root.contains("objects")) {
        return true;
    }
    if (!root["objects"].is_array()) {
        return false;
    }
    if (hasObjects) {
        *hasObjects = true;
    }

    std::vector<PlacementObject> loadedObjects;
    uint64_t maxId = 0;
    for (const nlohmann::json &objectJson : root["objects"]) {
        if (!objectJson.is_object()) {
            return false;
        }

        PlacementObject object{};
        object.id = objectJson.value("id", 0ull);

        const std::string kindName = objectJson.value("kind", std::string{});
        if (!kindName.empty()) {
            if (!IsPlacementObjectKind(kindName, &object.kind)) {
                return false;
            }
        } else {
            const std::string mapCode = objectJson.value("mapCode", std::string{"1"});
            switch (mapCode.empty() ? '1' : mapCode[0]) {
            case '2':
                object.kind = PlacementObjectKind::Wall;
                break;
            case '3':
                object.kind = PlacementObjectKind::PlayerStart;
                break;
            case '4':
                object.kind = PlacementObjectKind::GoalMarker;
                break;
            default:
                object.kind = PlacementObjectKind::Floor;
                break;
            }
        }

        object.name = objectJson.value("name", std::string{});
        object.gridX = objectJson.value("gridX", 0);
        object.gridY = objectJson.value("gridY", 0);
        const std::string mapCode =
            objectJson.value("mapCode", std::string{1, GetDefaultMapCode(object.kind)});
        object.mapCode = mapCode.empty() ? GetDefaultMapCode(object.kind) : mapCode[0];

        if (objectJson.contains("transform")) {
            const nlohmann::json &transformJson = objectJson["transform"];
            if (!transformJson.is_object()) {
                return false;
            }
            if (transformJson.contains("position") &&
                !ReadFloatArray(transformJson["position"], object.transform.position)) {
                return false;
            }
            if (transformJson.contains("rotation") &&
                !ReadRotationArray(transformJson["rotation"], object.transform.rotation)) {
                return false;
            }
            if (transformJson.contains("scale") &&
                !ReadFloatArray(transformJson["scale"], object.transform.scale)) {
                return false;
            }
        } else {
            object.transform.position = map_.GetCellCenter(object.gridX, object.gridY);
        }

        if (object.id == 0 || object.id <= maxId) {
            object.id = maxId + 1;
        }
        maxId = (std::max)(maxId, object.id);
        AssignRuntimeFields(object);
        loadedObjects.push_back(object);
    }

    objects_ = std::move(loadedObjects);
    nextObjectId_ = maxId + 1;
    if (selectedIndex_ >= static_cast<int>(objects_.size())) {
        selectedIndex_ = objects_.empty() ? 0 : static_cast<int>(objects_.size()) - 1;
    }
    return true;
}

bool GridPlacementTest::SaveSceneToJson(const std::string &path) const {
    std::filesystem::path scenePath(path);
    if (scenePath.has_parent_path()) {
        std::filesystem::create_directories(scenePath.parent_path());
    }

    nlohmann::json root;
    root["name"] = "game_stage";
    root["tileSize"] = map_.GetTileSize();
    root["legend"] = {
        {"0", "empty"},
        {"1", "floor"},
        {"2", "wall"},
        {"3", "player_start"},
        {"4", "goal_marker"},
    };
    root["rows"] = map_.GetRows();
    root["objects"] = nlohmann::json::array();

    for (const PlacementObject &object : objects_) {
        nlohmann::json objectJson;
        objectJson["id"] = object.id;
        objectJson["name"] = object.name;
        objectJson["kind"] = GetPlacementKindName(object.kind);
        objectJson["gridX"] = object.gridX;
        objectJson["gridY"] = object.gridY;
        objectJson["mapCode"] = std::string(1, object.mapCode);
        objectJson["transform"] = {
            {"position",
             {object.transform.position.x, object.transform.position.y,
              object.transform.position.z}},
            {"rotation",
             {object.transform.rotation.x, object.transform.rotation.y,
              object.transform.rotation.z, object.transform.rotation.w}},
            {"scale",
             {object.transform.scale.x, object.transform.scale.y,
              object.transform.scale.z}},
        };
        root["objects"].push_back(objectJson);
    }

    std::ofstream file(scenePath);
    if (!file) {
        return false;
    }

    file << root.dump(2);
    return true;
}

PlacementObject GridPlacementTest::CreatePlacementObject(PlacementObjectKind kind,
                                                         int gridX,
                                                         int gridY) {
    PlacementObject object{};
    object.id = AllocateObjectId();
    object.kind = kind;
    object.mapCode = GetDefaultMapCode(kind);
    object.gridX = gridX;
    object.gridY = gridY;
    object.name = GetPlacementKindDisplayName(kind);

    const float tileSize = map_.GetTileSize();
    switch (kind) {
    case PlacementObjectKind::Floor:
        object.modelId = floorModelId_;
        object.transform.position = map_.GetCellCenter(gridX, gridY, kFloorHeight);
        object.transform.rotation = MakeQuaternion(-XM_PIDIV2, 0.0f, 0.0f);
        object.transform.scale = {tileSize * floorScale_,
                                  tileSize * floorScale_, 1.0f};
        break;
    case PlacementObjectKind::Wall:
        object.modelId = wallModelId_;
        object.transform.position = map_.GetCellCenter(gridX, gridY, 0.05f);
        object.transform.rotation = MakeQuaternion(0.0f, XM_PIDIV4, 0.0f);
        object.transform.scale = {tileSize * wallScale_,
                                  tileSize * wallHeight_,
                                  tileSize * wallScale_};
        break;
    case PlacementObjectKind::PlayerStart:
        object.modelId = playerModelId_;
        object.transform.position = map_.GetCellCenter(gridX, gridY, 0.0f);
        object.transform.rotation = MakeQuaternion(0.0f, XM_PI, 0.0f);
        object.transform.scale = {0.24f, 0.24f, 0.24f};
        break;
    case PlacementObjectKind::GoalMarker:
        object.modelId = markerModelId_;
        object.transform.position = map_.GetCellCenter(gridX, gridY, kMarkerHeight);
        object.transform.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
        object.transform.scale = {tileSize * markerScale_,
                                  tileSize * (markerScale_ * 2.4f),
                                  tileSize * markerScale_};
        break;
    }

    return object;
}

void GridPlacementTest::EnsureFloorObjectAtCell(int gridX, int gridY) {
    const auto hasFloor =
        std::any_of(objects_.begin(), objects_.end(),
                    [gridX, gridY](const PlacementObject &object) {
                        return object.gridX == gridX && object.gridY == gridY &&
                               object.kind == PlacementObjectKind::Floor;
                    });
    if (hasFloor) {
        return;
    }

    PlacementObject floor =
        CreatePlacementObject(PlacementObjectKind::Floor, gridX, gridY);
    objects_.push_back(floor);
}

void GridPlacementTest::RemoveExistingPlayerStarts() {
    std::vector<std::pair<int, int>> touchedCells;
    objects_.erase(std::remove_if(objects_.begin(), objects_.end(),
                                  [&touchedCells](const PlacementObject &object) {
                                      if (object.kind !=
                                          PlacementObjectKind::PlayerStart) {
                                          return false;
                                      }
                                      touchedCells.push_back(
                                          {object.gridX, object.gridY});
                                      return true;
                                  }),
                   objects_.end());

    for (const auto &[gridX, gridY] : touchedCells) {
        SyncMapCellFromObjects(gridX, gridY);
    }
    ClampSelectedIndex();
}

void GridPlacementTest::SyncMapCellFromObjects(int gridX, int gridY) {
    char tile = '0';
    for (const PlacementObject &object : objects_) {
        if (object.gridX != gridX || object.gridY != gridY) {
            continue;
        }

        if (object.kind == PlacementObjectKind::PlayerStart) {
            tile = '3';
            break;
        }
        if (object.kind == PlacementObjectKind::GoalMarker) {
            tile = '4';
        } else if (object.kind == PlacementObjectKind::Wall && tile != '4') {
            tile = '2';
        } else if (object.kind == PlacementObjectKind::Floor && tile == '0') {
            tile = '1';
        }
    }

    map_.SetTile(gridX, gridY, tile);
    for (PlacementObject &object : objects_) {
        if (object.gridX == gridX && object.gridY == gridY) {
            object.mapCode = tile;
        }
    }
}

void GridPlacementTest::ClampSelectedIndex() {
    if (objects_.empty()) {
        selectedIndex_ = 0;
        return;
    }
    selectedIndex_ =
        (std::clamp)(selectedIndex_, 0, static_cast<int>(objects_.size()) - 1);
}

void GridPlacementTest::AssignRuntimeFields(PlacementObject &object) const {
    switch (object.kind) {
    case PlacementObjectKind::Floor:
        object.modelId = floorModelId_;
        if (object.name.empty()) {
            object.name = "Floor";
        }
        break;
    case PlacementObjectKind::Wall:
        object.modelId = wallModelId_;
        if (object.name.empty()) {
            object.name = "Wall";
        }
        break;
    case PlacementObjectKind::PlayerStart:
        object.modelId = playerModelId_;
        if (object.name.empty()) {
            object.name = "Player Start";
        }
        break;
    case PlacementObjectKind::GoalMarker:
        object.modelId = markerModelId_;
        if (object.name.empty()) {
            object.name = "GoalMarker";
        }
        break;
    }

    object.mapCode = object.mapCode == '0' ? GetDefaultMapCode(object.kind)
                                           : object.mapCode;
    NormalizeQuaternion(object.transform.rotation);
    object.transform.scale.x = (std::max)(0.01f, object.transform.scale.x);
    object.transform.scale.y = (std::max)(0.01f, object.transform.scale.y);
    object.transform.scale.z = (std::max)(0.01f, object.transform.scale.z);
}

uint64_t GridPlacementTest::AllocateObjectId() {
    return nextObjectId_++;
}

void GridPlacementTest::ResetPlayerToSpawn() {
    playerTransform_.position = playerSpawn_;
    playerTransform_.rotation = MakeQuaternion(0.0f, XM_PI, 0.0f);
    playerTransform_.scale = {0.34f, 0.34f, 0.34f};
    playerVelocityY_ = 0.0f;
    playerOnGround_ = true;
    cameraTarget_ = playerSpawn_;
}

bool GridPlacementTest::IsBlocked(float worldX, float worldZ) const {
    const float tileSize = map_.GetTileSize();
    if (tileSize <= 0.0f) {
        return false;
    }

    const float originX = -static_cast<float>(map_.GetWidth() - 1) * tileSize * 0.5f;
    const float originZ = -static_cast<float>(map_.GetHeight() - 1) * tileSize * 0.5f;
    const int x =
        static_cast<int>(std::round((worldX - originX) / tileSize));
    const int y =
        static_cast<int>(std::round((worldZ - originZ) / tileSize));
    const char tile = map_.GetTile(x, y);
    return tile == '0' || tile == '2';
}

void GridPlacementTest::SelectObject(int offset) {
    if (objects_.empty()) {
        selectedIndex_ = 0;
        return;
    }

    const int count = static_cast<int>(objects_.size());
    selectedIndex_ = (selectedIndex_ + offset + count) % count;
}

const char *GridPlacementTest::GetKindName(PlacementObjectKind kind) {
    return GetPlacementKindName(kind);
}

char GridPlacementTest::GetBrushTile(int brush) {
    switch (brush) {
    case 0:
        return '1';
    case 1:
        return '2';
    case 2:
        return '3';
    case 3:
        return '4';
    default:
        return '1';
    }
}

XMFLOAT4 GridPlacementTest::MakeQuaternion(float pitch, float yaw, float roll) {
    XMFLOAT4 result{};
    XMStoreFloat4(&result, XMQuaternionRotationRollPitchYaw(pitch, yaw, roll));
    return result;
}
