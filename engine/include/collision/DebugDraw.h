#pragma once
#include "AABB.h"
#include "OBB.h"
#include "Transform.h"
#include <cstdint>

class Camera;
class ModelAssets;
class ModelRenderer;

class DebugDraw {
  public:
    void Initialize(uint32_t boxModelId) { boxModelId_ = boxModelId; }
    void DrawOBB(const ModelAssets *modelAssets, ModelRenderer *modelRenderer,
                 const OBB &obb, const Camera &camera);
    void DrawAABB(const ModelAssets *modelAssets, ModelRenderer *modelRenderer,
                  const AABB &aabb, const Camera &camera);

  private:
    uint32_t boxModelId_ = 0;
};
