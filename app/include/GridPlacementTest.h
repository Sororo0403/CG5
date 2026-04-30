#pragma once
#include "Camera.h"
#include "IEditableObject.h"
#include "IEditableScene.h"
#include "PlacementMap.h"
#include "SceneContext.h"
#include "SceneSerializer.h"
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
    std::string collider;
    bool locked = false;
    bool visible = true;

    EditableObjectDesc GetEditorDesc() const override;
    void SetEditorName(const std::string &name) override;
    bool SetEditorCollider(const std::string &collider) override;
    bool SetEditorLocked(bool locked) override;
    bool SetEditorVisible(bool visible) override;
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
    uint64_t GetSelectedObjectId() const override;
    void SetSelectedObjectById(uint64_t id) override;
    int GetSelectedEditableObjectIndex() const override;
    void SetSelectedEditableObjectIndex(int index) override;
    void OnEditableObjectChanged(size_t index) override;
    bool SaveScene(std::string *message) override;
    bool LoadScene(std::string *message) override;
    bool SaveSceneAs(const std::string &path, std::string *message) override;
    bool LoadSceneFromPath(const std::string &path,
                           std::string *message) override;
    std::string GetCurrentScenePath() const override;
    std::string GetCurrentSceneName() const override;
    bool CaptureSceneState(std::string *outState,
                           std::string *message) const override;
    bool RestoreSceneState(const std::string &state,
                           std::string *message) override;
    bool IsSceneDirty() const override;
    void MarkSceneDirty() override;
    void ClearSceneDirty() override;
    bool CanEditObjects() const override;
    bool AddEditableObject(const std::string &type,
                           std::string *message) override;
    bool DeleteSelectedEditableObject(std::string *message) override;
    bool DuplicateSelectedEditableObject(std::string *message) override;
    void OnEnterEditorMode() override;
    void OnEnterGameplayMode() override;
    void SetGameplayPaused(bool paused) override;
    bool IsGameplayPaused() const override;
    void ResetGameplay() override;

    void EnterEditorMode();
    void EnterGameplayMode();
    void PauseGameplay(bool paused);

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
    EditableSceneDocument BuildSceneDocument() const;
    bool ApplySceneDocument(const EditableSceneDocument &document,
                            std::string *message);
    void SetCurrentScenePath(const std::string &path);
    void RegisterEditableObjectTypes();
    IEditableObject *CreateEditableObjectFromFactory(const std::string &type);
    PlacementObject CreatePlacementObject(PlacementObjectKind kind, int gridX,
                                          int gridY);
    void EnsureFloorObjectAtCell(int gridX, int gridY);
    void RemoveExistingPlayerStarts();
    void SyncMapCellFromObjects(int gridX, int gridY);
    void ClampSelectedIndex();
    int FindObjectIndexById(uint64_t id) const;
    void EnsureSelectedObjectValid();
    void AssignRuntimeFields(PlacementObject &object) const;
    uint64_t AllocateObjectId();
    bool IsBlocked(float worldX, float worldZ) const;
    void SelectObject(int offset);
    static char GetBrushTile(int brush);
    static DirectX::XMFLOAT4 MakeQuaternion(float pitch, float yaw, float roll);

    PlacementMap map_;
    std::vector<PlacementObject> objects_;
    uint64_t selectedObjectId_ = 0;
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
    bool gameplayPaused_ = false;
    bool sceneDirty_ = false;
    DirectX::XMFLOAT3 editorCameraTarget_{0.0f, 0.0f, 0.0f};
    float editorCameraYaw_ = 0.0f;
    float editorCameraDistance_ = 9.5f;
    float editorCameraHeight_ = 7.0f;
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
    std::string currentScenePath_ = "resources/levels/game_stage.json";
    std::string currentSceneName_ = "game_stage";
    float lastAppliedTileSize_ = 1.0f;
    float lastAppliedFloorScale_ = 1.0f;
    float lastAppliedWallScale_ = 0.82f;
    float lastAppliedWallHeight_ = 0.9f;
    float lastAppliedMarkerScale_ = 0.5f;
};
