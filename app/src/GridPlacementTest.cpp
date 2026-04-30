#include "GridPlacementTest.h"
#include "DirectXCommon.h"
#include "Input.h"
#include "Material.h"
#include "Model.h"
#include "ModelManager.h"
#include "ModelRenderer.h"
#include "TextureManager.h"
#ifdef _DEBUG
#include "imgui.h"
#endif // _DEBUG
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

using namespace DirectX;

namespace {

constexpr float kFloorHeight = 0.0f;
constexpr float kMarkerHeight = 0.08f;

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

void GridPlacementTest::Initialize(const SceneContext &ctx) {
    CreateModels(ctx);
    BuildObjects();
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

    const XMFLOAT4 floorRotation = MakeQuaternion(-XM_PIDIV2, 0.0f, 0.0f);
    const float tileSize = map_.GetTileSize();

    for (int y = 0; y < map_.GetHeight(); ++y) {
        for (int x = 0; x < map_.GetWidth(); ++x) {
            const char tile = map_.GetTile(x, y);
            if (tile == '0') {
                continue;
            }

            PlacementObject floor{};
            floor.kind = PlacementObjectKind::Floor;
            floor.mapCode = tile;
            floor.gridX = x;
            floor.gridY = y;
            floor.modelId = floorModelId_;
            floor.transform.position = map_.GetCellCenter(x, y, kFloorHeight);
            floor.transform.rotation = floorRotation;
            floor.transform.scale = {tileSize, tileSize, 1.0f};
            floor.name = "Floor";
            objects_.push_back(floor);

            if (tile == '2') {
                PlacementObject wall{};
                wall.kind = PlacementObjectKind::Wall;
                wall.mapCode = tile;
                wall.gridX = x;
                wall.gridY = y;
                wall.modelId = wallModelId_;
                wall.transform.position = map_.GetCellCenter(x, y, 0.05f);
                wall.transform.rotation = MakeQuaternion(0.0f, XM_PIDIV4, 0.0f);
                wall.transform.scale = {tileSize * 0.82f, tileSize * 0.9f,
                                        tileSize * 0.82f};
                wall.name = "Wall";
                objects_.push_back(wall);
            } else if (tile == '3') {
                PlacementObject player{};
                player.kind = PlacementObjectKind::PlayerStart;
                player.mapCode = tile;
                player.gridX = x;
                player.gridY = y;
                player.modelId = playerModelId_;
                player.transform.position = map_.GetCellCenter(x, y, 0.0f);
                player.transform.rotation = MakeQuaternion(0.0f, XM_PI, 0.0f);
                player.transform.scale = {0.34f, 0.34f, 0.34f};
                player.name = "Player Start";
                objects_.push_back(player);
            } else if (tile == '4') {
                PlacementObject marker{};
                marker.kind = PlacementObjectKind::EnemyMarker;
                marker.mapCode = tile;
                marker.gridX = x;
                marker.gridY = y;
                marker.modelId = markerModelId_;
                marker.transform.position =
                    map_.GetCellCenter(x, y, kMarkerHeight);
                marker.transform.scale = {tileSize * 0.5f, tileSize * 1.2f,
                                          tileSize * 0.5f};
                marker.name = "Enemy Marker";
                objects_.push_back(marker);
            }
        }
    }

    if (selectedIndex_ >= static_cast<int>(objects_.size())) {
        selectedIndex_ = objects_.empty() ? 0 : static_cast<int>(objects_.size()) - 1;
    }
}

void GridPlacementTest::Update(const SceneContext &ctx, Camera &camera) {
    if (ctx.assets.model) {
        ctx.assets.model->UpdateAnimation(playerModelId_, ctx.frame.deltaTime);
    }
    UpdateCamera(ctx, camera);
}

void GridPlacementTest::UpdateCamera(const SceneContext &ctx, Camera &camera) {
    const Input *input = ctx.core.input;
    if (!input) {
        return;
    }

    const float dt = ctx.frame.deltaTime;
    const float moveSpeed = 4.5f;
    const float zoomSpeed = 7.0f;
    const float rotateSpeed = 1.6f;

    if (input->IsKeyPress(DIK_A)) {
        cameraTarget_.x -= moveSpeed * dt;
    }
    if (input->IsKeyPress(DIK_D)) {
        cameraTarget_.x += moveSpeed * dt;
    }
    if (input->IsKeyPress(DIK_W)) {
        cameraTarget_.z += moveSpeed * dt;
    }
    if (input->IsKeyPress(DIK_S)) {
        cameraTarget_.z -= moveSpeed * dt;
    }
    if (input->IsKeyPress(DIK_Q)) {
        cameraDistance_ += zoomSpeed * dt;
    }
    if (input->IsKeyPress(DIK_E)) {
        cameraDistance_ -= zoomSpeed * dt;
    }
    if (input->IsKeyPress(DIK_LEFT)) {
        cameraYaw_ -= rotateSpeed * dt;
    }
    if (input->IsKeyPress(DIK_RIGHT)) {
        cameraYaw_ += rotateSpeed * dt;
    }
    if (input->IsKeyPress(DIK_UP)) {
        cameraHeight_ += moveSpeed * dt;
    }
    if (input->IsKeyPress(DIK_DOWN)) {
        cameraHeight_ -= moveSpeed * dt;
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

    renderer->PostDraw();
}

void GridPlacementTest::DrawDebugUI(Camera &camera) {
#ifdef _DEBUG
    if (ImGui::Begin("Grid Placement Test")) {
        ImGui::Text("Map Size: %d x %d", map_.GetWidth(), map_.GetHeight());
        ImGui::Text("Tile Size: %.2f", map_.GetTileSize());
        ImGui::Text("Placed Objects: %zu", objects_.size());
        const XMFLOAT3 &cameraPosition = camera.GetPosition();
        ImGui::Text("Camera Position: %.2f, %.2f, %.2f", cameraPosition.x,
                    cameraPosition.y, cameraPosition.z);
        ImGui::Text("Camera Target: %.2f, %.2f, %.2f", cameraTarget_.x,
                    cameraTarget_.y, cameraTarget_.z);
        ImGui::Separator();
        ImGui::TextUnformatted("WASD: Move target");
        ImGui::TextUnformatted("Q/E: Zoom");
        ImGui::TextUnformatted("Arrow Left/Right: Rotate");
        ImGui::TextUnformatted("Arrow Up/Down: Height");
        ImGui::TextUnformatted("Tab / Shift+Tab: Select object");
        ImGui::Separator();

        if (!objects_.empty()) {
            selectedIndex_ =
                (std::clamp)(selectedIndex_, 0, static_cast<int>(objects_.size()) - 1);
            PlacementObject &selected = objects_[static_cast<size_t>(selectedIndex_)];
            ImGui::Text("Selected: %d / %zu", selectedIndex_ + 1,
                        objects_.size());
            ImGui::Text("Name: %s", selected.name.c_str());
            ImGui::Text("Kind: %s", GetKindName(selected.kind));
            ImGui::Text("Map Code: %c  Grid: (%d, %d)", selected.mapCode,
                        selected.gridX, selected.gridY);
            ImGui::DragFloat3("Position", &selected.transform.position.x,
                              0.05f);
            ImGui::DragFloat3("Scale", &selected.transform.scale.x, 0.02f,
                              0.01f, 20.0f);
            if (ImGui::Button("Previous")) {
                SelectObject(-1);
            }
            ImGui::SameLine();
            if (ImGui::Button("Next")) {
                SelectObject(1);
            }
        }
    }
    ImGui::End();
#else
    (void)camera;
#endif // _DEBUG
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
    switch (kind) {
    case PlacementObjectKind::Floor:
        return "Floor";
    case PlacementObjectKind::Wall:
        return "Wall";
    case PlacementObjectKind::PlayerStart:
        return "Player Start";
    case PlacementObjectKind::EnemyMarker:
        return "Enemy Marker";
    default:
        return "Unknown";
    }
}

XMFLOAT4 GridPlacementTest::MakeQuaternion(float pitch, float yaw, float roll) {
    XMFLOAT4 result{};
    XMStoreFloat4(&result, XMQuaternionRotationRollPitchYaw(pitch, yaw, roll));
    return result;
}
