#pragma once

class Camera;
class ModelAssets;
class ModelRenderer;
class World;

/// <summary>
/// MeshRendererとTransformを持つEntityを描画するSystem。
/// </summary>
struct RenderSystem {
    /// <summary>
    /// World内の描画対象EntityをModelRendererで描画する。
    /// </summary>
    /// <param name="world">描画対象のWorld。</param>
    /// <param name="modelAssets">モデルデータを取得するModelAssets。</param>
    /// <param name="modelRenderer">モデル描画を行うModelRenderer。</param>
    /// <param name="camera">描画に使用するCamera。</param>
    void Draw(World &world, const ModelAssets &modelAssets,
              ModelRenderer &modelRenderer, const Camera &camera);
};
