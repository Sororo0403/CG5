#pragma once

class Camera;
class ModelManager;
class World;

/// <summary>
/// MeshRendererとTransformを持つEntityを描画するSystem。
/// </summary>
struct RenderSystem {
    /// <summary>
    /// World内の描画対象EntityをModelManagerで描画する。
    /// </summary>
    /// <param name="world">描画対象のWorld。</param>
    /// <param name="modelManager">モデル描画を行うModelManager。</param>
    /// <param name="camera">描画に使用するCamera。</param>
    void Draw(World &world, ModelManager &modelManager, const Camera &camera);
};
