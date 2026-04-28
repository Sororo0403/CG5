#pragma once
#include "Camera.h"
#include "LightManager.h"
#include "MaterialManager.h"
#include "Model.h"
#include "Transform.h"
#include <d3d12.h>
#include <wrl.h>

class DirectXCommon;
class SrvManager;
class MeshManager;
class TextureManager;

/// <summary>
/// モデル描画時の一時エフェクト設定
/// </summary>
struct ModelDrawEffect {
    bool enabled = false;
    bool additiveBlend = false;
    bool disableCulling = false;
    DirectX::XMFLOAT4 color = {1.0f, 0.2f, 0.7f, 0.65f};
    float intensity = 0.0f;
    float fresnelPower = 3.5f;
    float noiseAmount = 0.0f;
    float time = 0.0f;
};

/// <summary>
/// モデル描画パイプラインと定数バッファ更新を担当する
/// </summary>
class ModelRenderer {
  public:
    /// <summary>
    /// 初期化処理
    /// </summary>
    /// <param name="dxCommon">DirectX共通管理</param>
    /// <param name="srvManager">SRV管理</param>
    /// <param name="meshManager">メッシュ管理</param>
    /// <param name="textureManager">テクスチャ管理</param>
    /// <param name="materialManager">マテリアル管理</param>
    void Initialize(DirectXCommon *dxCommon, SrvManager *srvManager,
                    MeshManager *meshManager, TextureManager *textureManager,
                    MaterialManager *materialManager);

    /// <summary>
    /// 描画処理
    /// </summary>
    /// <param name="model">描画するモデル</param>
    /// <param name="transform">描画するモデルのTransform</param>
    /// <param name="camera">描画に使用するカメラ</param>
    /// <param name="environmentTextureId">
    /// この描画で使用する環境マップテクスチャID
    /// UINT32_MAXを指定した場合はSetEnvironmentTextureの設定を使用
    /// </param>
    void Draw(const Model &model, const Transform &transform,
              const Camera &camera,
              uint32_t environmentTextureId = UINT32_MAX);

    /// <summary>
    /// 現在フレームの描画エフェクトを設定する
    /// </summary>
    /// <param name="effect">適用するエフェクト</param>
    void SetDrawEffect(const ModelDrawEffect &effect) { currentEffect_ = effect; }
    /// <summary>
    /// 描画エフェクト設定を初期状態へ戻す
    /// </summary>
    void ClearDrawEffect() { currentEffect_ = ModelDrawEffect{}; }
    /// <summary>
    /// シーンライティングを設定する
    /// </summary>
    /// <param name="lighting">適用するライティング定数</param>
    void SetSceneLighting(const SceneLighting &lighting) {
        currentLighting_ = lighting;
    }
    /// <summary>
    /// 環境マップに使うキューブマップテクスチャを設定する
    /// </summary>
    /// <param name="textureId">キューブマップのテクスチャID</param>
    void SetEnvironmentTexture(uint32_t textureId) {
        environmentTextureId_ = textureId;
        hasEnvironmentTexture_ = true;
    }
    /// <summary>
    /// ディゾルブ用ノイズテクスチャを設定する
    /// </summary>
    /// <param name="textureId">2DノイズテクスチャID</param>
    void SetDissolveNoiseTexture(uint32_t textureId) {
        dissolveNoiseTextureId_ = textureId;
    }
    /// <summary>
    /// 環境マップを無効化する
    /// </summary>
    void ClearEnvironmentTexture() { hasEnvironmentTexture_ = false; }
    /// <summary>
    /// モデル用スキンクラスターGPUリソースを生成する
    /// </summary>
    /// <param name="model">対象モデル</param>
    void CreateSkinClusters(Model &model);
    /// <summary>
    /// モデル用スキンクラスターのSRV/UAVディスクリプタを解放する
    /// </summary>
    /// <param name="model">対象モデル</param>
    void ReleaseSkinClusters(Model &model);
    /// <summary>
    /// スキンクラスターのパレット内容を更新する
    /// </summary>
    /// <param name="model">対象モデル</param>
    void UpdateSkinClusters(Model &model);

    /// <summary>
    /// 描画前処理
    /// </summary>
    void PreDraw();

    /// <summary>
    /// 描画後処理
    /// </summary>
    void PostDraw();

  private:
    /// <summary>
    /// ルートシグネチャを生成する
    /// </summary>
    void CreateRootSignature();
    /// <summary>
    /// ComputeShader用ルートシグネチャを生成する
    /// </summary>
    void CreateSkinningRootSignature();
    /// <summary>
    /// パイプラインステートを生成する
    /// </summary>
    void CreatePipelineState();
    /// <summary>
    /// ComputeShader用パイプラインステートを生成する
    /// </summary>
    void CreateSkinningPipelineState();
    /// <summary>
    /// 定数バッファを生成する
    /// </summary>
    void CreateConstantBuffer();
    /// <summary>
    /// ComputeShaderでスキニング済み頂点を書き込む
    /// </summary>
    void DispatchSkinning(const ModelSubMesh &subMesh);

  private:
    static constexpr uint32_t kMaxDraws = 4096;

    DirectXCommon *dxCommon_ = nullptr;
    SrvManager *srvManager_ = nullptr;
    MeshManager *meshManager_ = nullptr;
    TextureManager *textureManager_ = nullptr;
    MaterialManager *materialManager_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> skinningRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> opaquePSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> transparentPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> additivePSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> additiveNoCullPSO_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> skinningPSO_;
    Microsoft::WRL::ComPtr<ID3D12Resource> objectConstBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> sceneConstBuffer_;

    uint32_t drawIndex_ = 0;
    bool drawLimitWarningIssued_ = false;
    uint32_t objectCbStride_ = 0;
    uint32_t sceneCbStride_ = 0;
    uint8_t *mappedObjectCB_ = nullptr;
    uint8_t *mappedSceneCB_ = nullptr;
    ModelDrawEffect currentEffect_{};
    SceneLighting currentLighting_{};
    uint32_t environmentTextureId_ = 0;
    uint32_t dissolveNoiseTextureId_ = 0;
    bool hasEnvironmentTexture_ = false;
};
