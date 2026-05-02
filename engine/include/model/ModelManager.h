#pragma once
#include "ModelAssets.h"
#include "ModelRenderer.h"
#include "SkeletonDebugRenderer.h"
#include "ModelAnimationController.h"
#include <string>

class DirectXCommon;
class SrvManager;
class TextureManager;

/// <summary>
/// モデル資産、描画、アニメーションを束ねる互換用Facade。
/// 新規コードではModelAssets、ModelRenderer、ModelAnimationControllerを直接使う。
/// </summary>
class ModelManager {
  public:
    ~ModelManager();

    void Initialize(DirectXCommon *dxCommon, SrvManager *srvManager,
                    TextureManager *textureManager);

    uint32_t Load(const DirectXCommon::UploadContext &uploadContext,
                  const std::wstring &path);

    uint32_t CreatePlane(uint32_t textureId, const Material &material);
    uint32_t CreateRing(uint32_t textureId, const Material &material,
                        uint32_t divide = 32, float outerRadius = 1.0f,
                        float innerRadius = 0.2f);
    uint32_t CreateCylinder(uint32_t textureId, const Material &material,
                            uint32_t divide = 32, float topRadius = 1.0f,
                            float bottomRadius = 1.0f, float height = 3.0f);

    void UpdateAnimation(uint32_t modelId, float deltaTime);
    void PlayAnimation(uint32_t modelId, const std::string &animationName,
                       bool loop = true);
    bool IsAnimationFinished(uint32_t modelId) const;

    Model *GetModel(uint32_t modelId);
    const Model *GetModel(uint32_t modelId) const;

    const Material &GetMaterial(uint32_t materialId) const;
    void SetMaterial(uint32_t materialId, const Material &material);

    void DrawSkeleton(uint32_t modelId, const Transform &transform,
                      const Camera &camera);

    ModelAssets *GetAssets() { return &assets_; }
    const ModelAssets *GetAssets() const { return &assets_; }

    ModelRenderer *GetRenderer() { return &modelRenderer_; }
    const ModelRenderer *GetRenderer() const { return &modelRenderer_; }

    ModelAnimationController *GetAnimationController() { return &animation_; }
    const ModelAnimationController *GetAnimationController() const {
        return &animation_;
    }

    void PreDraw() { modelRenderer_.PreDraw(); }
    void PostDraw() { modelRenderer_.PostDraw(); }
    void Draw(uint32_t modelId, const Transform &transform,
              const Camera &camera);
    void SetDrawEffect(const ModelDrawEffect &effect) {
        modelRenderer_.SetDrawEffect(effect);
    }
    void ClearDrawEffect() { modelRenderer_.ClearDrawEffect(); }
    void SetSceneLighting(const SceneLighting &lighting) {
        modelRenderer_.SetSceneLighting(lighting);
    }

  private:
    void ReleaseModelRendererResources();

    ModelAssets assets_;
    ModelRenderer modelRenderer_;
    SkeletonDebugRenderer skeletonDebugRenderer_;
    ModelAnimationController animation_;
};
