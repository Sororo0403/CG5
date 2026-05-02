#pragma once
#include <cstdint>
#include <memory>

class DirectXCommon;
class ModelAnimationController;
class ModelAssets;
class ModelRenderer;
class SkeletonDebugRenderer;
class SrvManager;
class TextureManager;

/// <summary>
/// モデル関連サービスの所有と初期化順、描画用GPUリソース寿命を管理する。
/// </summary>
class ModelServices {
  public:
    ModelServices();
    ~ModelServices();

    ModelServices(const ModelServices &) = delete;
    ModelServices &operator=(const ModelServices &) = delete;
    ModelServices(ModelServices &&) = delete;
    ModelServices &operator=(ModelServices &&) = delete;

    void Initialize(DirectXCommon *dxCommon, SrvManager *srvManager,
                    TextureManager *textureManager);

    ModelAssets &Assets() { return *assets_; }
    const ModelAssets &Assets() const { return *assets_; }

    ModelRenderer &Renderer() { return *renderer_; }
    const ModelRenderer &Renderer() const { return *renderer_; }

    ModelAnimationController &Animation() { return *animation_; }
    const ModelAnimationController &Animation() const { return *animation_; }

    SkeletonDebugRenderer &SkeletonDebug() { return *skeletonDebug_; }
    const SkeletonDebugRenderer &SkeletonDebug() const {
        return *skeletonDebug_;
    }

    bool PrepareModel(uint32_t modelId);
    bool UpdateAnimation(uint32_t modelId, float deltaTime);
    void ReleaseRenderResources();

  private:
    std::unique_ptr<ModelAssets> assets_;
    std::unique_ptr<ModelRenderer> renderer_;
    std::unique_ptr<ModelAnimationController> animation_;
    std::unique_ptr<SkeletonDebugRenderer> skeletonDebug_;
};
