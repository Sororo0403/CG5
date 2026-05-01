#pragma once
#include "AABB.h"
#include "OBB.h"
#include "Transform.h"
#include <cstdint>

class Camera;
class ModelManager;

class DebugDraw {
  public:
    void Initialize(uint32_t boxModelId) { boxModelId_ = boxModelId; }
    void DrawOBB(ModelManager *modelManager, const OBB &obb,
                 const Camera &camera);
    void DrawAABB(ModelManager *modelManager, const AABB &aabb,
                  const Camera &camera);

  private:
    uint32_t boxModelId_ = 0;
};
