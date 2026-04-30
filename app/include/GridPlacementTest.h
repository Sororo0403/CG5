#pragma once
#include "Camera.h"
#include "IEditableObject.h"
#include "IEditableScene.h"
#include "PlacementMap.h"
#include "SceneContext.h"
#include "Transform.h"
#include <DirectXMath.h>
#include <cstdint>
#include <string>
#include <vector>

class ModelRenderer;

enum class PlacementObjectKind {
    Floor,
    Wall,
    PlayerStart,
    GoalMarker,
};

struct PlacementObject : public IEditableObject {
    uint64_t id = 0;
    PlacementObjectKind kind = PlacementObjectKind::Floor;
    char mapCode = '0';
    int gridX = 0;
    int gridY = 0;
    uint32_t modelId = 0;
    Transform transform;
    std::string name;

    EditableObjectDesc GetEditorDesc() const override;
    void SetEditorName(const std::string &name) override;
    EditableTransform GetEditorTransform() const override;
    void SetEditorTransform(const EditableTransform &transform) override;
};

class GridPlacementTest : public IEditableScene {
  public:
    void Initialize(const SceneContext &ctx);
    void Update(const SceneContext &ctx, Camera &camera);
    void DrawGBuffer(const SceneContext &ctx, const Camera &camera);
    void DrawForward(const SceneContext &ctx, const Camera &camera,
                     uint32_t environmentTextureId);
    void RegisterDebugUI();
    void RegisterInspectorObjects(const Camera &camera);

    const PlacementMap &GetMap() const { return map_; }
    size_t GetObjectCount() const { return GetEditableObjectCount(); }
    const DirectX::XMFLOAT3 &GetCameraTarget() const { return cameraTarget_; }
    static const char *GetKindName(PlacementObjectKind kind);

    size_t GetEditableObjectCount() const override;
    IEditableObject *GetEditableObject(size_t index) override;
    const IEditableObject *GetEditableObject(size_t index) const override;
    int GetSelectedEditableObjectIndex() const override;
    void SetSelectedEditableObjectIndex(int index) override;
    void OnEditableObjectChanged(size_t index) override;
    bool SaveScene(std::string *message) override;
    bool LoadScene(std::string *message) override;

  private:
    void CreateModels(const SceneContext &ctx);
    void BuildObjects();
    void UpdateCamera(const SceneContext &ctx, Camera &camera);
    void UpdateTuningMode(const SceneContext &ctx, Camera &camera);
    void UpdatePlayMode(const SceneContext &ctx, Camera &camera);
    void UpdatePlacementInput(const SceneContext &ctx);
    void UpdateObjectTuning();
    void DrawObjects(ModelRenderer *renderer, const SceneContext &ctx,
                     const Camera &camera, bool gBuffer,
                     uint32_t environmentTextureId);
    void DrawPlayer(ModelRenderer *renderer, const SceneContext &ctx,
                    const Camera &camera, bool gBuffer,
                    uint32_t environmentTextureId);
    void ResetPlayerToSpawn();
    void RecomputePlayerSpawnFromObjects();
    bool LoadObjectsFromJson(const std::string &path, bool *hasObjects);
    bool SaveSceneToJson(const std::string &path) const;
    void AssignRuntimeFields(PlacementObject &object) const;
    uint64_t AllocateObjectId();
    bool IsBlocked(float worldX, float worldZ) const;
    void SelectObject(int offset);
    static char GetBrushTile(int brush);
    static DirectX::XMFLOAT4 MakeQuaternion(float pitch, float yaw, float roll);

    PlacementMap map_;
    std::vector<PlacementObject> objects_;
    int selectedIndex_ = 0;
    uint32_t floorModelId_ = 0;
    uint32_t wallModelId_ = 0;
    uint32_t playerModelId_ = 0;
    uint32_t markerModelId_ = 0;
    DirectX::XMFLOAT3 cameraTarget_{0.0f, 0.0f, 0.0f};
    float cameraYaw_ = 0.0f;
    float cameraDistance_ = 9.5f;
    float cameraHeight_ = 7.0f;
    Transform playerTransform_;
    DirectX::XMFLOAT3 playerSpawn_{0.0f, 0.0f, 0.0f};
    float playerVelocityY_ = 0.0f;
    bool playerOnGround_ = true;
    float playerMoveSpeed_ = 4.0f;
    float playerJumpStrength_ = 6.0f;
    float cameraFollowSpeed_ = 8.0f;
    float freeCameraMoveSpeed_ = 4.5f;
    float freeCameraZoomSpeed_ = 7.0f;
    float freeCameraRotateSpeed_ = 1.6f;
    float placementTileSize_ = 1.0f;
    float floorScale_ = 1.0f;
    float wallScale_ = 0.82f;
    float wallHeight_ = 0.9f;
    float markerScale_ = 0.5f;
    int placementCursorX_ = 1;
    int placementCursorY_ = 1;
    int placementBrush_ = 1;
    int saveCount_ = 0;
    int loadCount_ = 0;
    uint64_t nextObjectId_ = 1;
    std::string stagePath_ = "resources/levels/game_stage.json";
    float lastAppliedTileSize_ = 1.0f;
    float lastAppliedFloorScale_ = 1.0f;
    float lastAppliedWallScale_ = 0.82f;
    float lastAppliedWallHeight_ = 0.9f;
    float lastAppliedMarkerScale_ = 0.5f;
};
