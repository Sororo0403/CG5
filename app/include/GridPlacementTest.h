#pragma once
#include "Camera.h"
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
    EnemyMarker,
};

struct PlacementObject {
    PlacementObjectKind kind = PlacementObjectKind::Floor;
    char mapCode = '0';
    int gridX = 0;
    int gridY = 0;
    uint32_t modelId = 0;
    Transform transform;
    std::string name;
};

class GridPlacementTest {
  public:
    void Initialize(const SceneContext &ctx);
    void Update(const SceneContext &ctx, Camera &camera);
    void DrawGBuffer(const SceneContext &ctx, const Camera &camera);
    void DrawForward(const SceneContext &ctx, const Camera &camera,
                     uint32_t environmentTextureId);
    void DrawDebugUI(Camera &camera);

    const PlacementMap &GetMap() const { return map_; }
    size_t GetObjectCount() const { return objects_.size(); }
    const DirectX::XMFLOAT3 &GetCameraTarget() const { return cameraTarget_; }

  private:
    void CreateModels(const SceneContext &ctx);
    void BuildObjects();
    void UpdateCamera(const SceneContext &ctx, Camera &camera);
    void DrawObjects(ModelRenderer *renderer, const SceneContext &ctx,
                     const Camera &camera, bool gBuffer,
                     uint32_t environmentTextureId);
    void SelectObject(int offset);
    static const char *GetKindName(PlacementObjectKind kind);
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
};
