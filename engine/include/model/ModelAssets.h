#pragma once
#include "AssimpLoader.h"
#include "MaterialManager.h"
#include "MeshManager.h"
#include "Model.h"
#include <string>
#include <vector>

class DirectXCommon;
class TextureManager;

/// <summary>
/// モデルデータ、メッシュ、マテリアルの読み込みと生成を管理する。
/// </summary>
class ModelAssets {
  public:
    /// <summary>
    /// GPUリソース生成に使う依存先を設定し、保持済みモデルを破棄する。
    /// </summary>
    void Initialize(DirectXCommon *dxCommon, TextureManager *textureManager);

    /// <summary>
    /// モデルファイルを読み込む。
    /// </summary>
    uint32_t Load(const DirectXCommon::UploadContext &uploadContext,
                  const std::wstring &path);

    /// <summary>
    /// XY平面のPrimitiveを生成する。
    /// </summary>
    uint32_t CreatePlane(uint32_t textureId, const Material &material);

    /// <summary>
    /// XY平面のRing Primitiveを生成する。
    /// </summary>
    uint32_t CreateRing(uint32_t textureId, const Material &material,
                        uint32_t divide = 32, float outerRadius = 1.0f,
                        float innerRadius = 0.2f);

    /// <summary>
    /// Y軸方向に伸びる筒状Cylinder Primitiveを生成する。
    /// </summary>
    uint32_t CreateCylinder(uint32_t textureId, const Material &material,
                            uint32_t divide = 32, float topRadius = 1.0f,
                            float bottomRadius = 1.0f, float height = 3.0f);

    Model *GetModel(uint32_t modelId);
    const Model *GetModel(uint32_t modelId) const;

    const Material &GetMaterial(uint32_t materialId) const;
    void SetMaterial(uint32_t materialId, const Material &material);

    MeshManager *GetMeshManager() { return &meshManager_; }
    const MeshManager *GetMeshManager() const { return &meshManager_; }
    MaterialManager *GetMaterialManager() { return &materialManager_; }
    const MaterialManager *GetMaterialManager() const {
        return &materialManager_;
    }

    std::vector<Model> &Models() { return models_; }
    const std::vector<Model> &Models() const { return models_; }

    bool IsValidModelId(uint32_t modelId, const char *caller) const;
    void Clear();

  private:
    DirectXCommon *dxCommon_ = nullptr;
    TextureManager *textureManager_ = nullptr;

    MeshManager meshManager_;
    MaterialManager materialManager_;
    AssimpLoader assimpLoader_;
    std::vector<Model> models_;
};
