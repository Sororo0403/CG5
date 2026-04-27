#pragma once
#include <d3d12.h>
#include <cstdint>
#include <wrl.h>

class DirectXCommon;
class SrvManager;

/// <summary>
/// テクスチャを画面全体へ描画するポストエフェクト用レンダラー
/// </summary>
class PostEffectRenderer {
  public:
    enum class Mode : int32_t {
        None = 0,
        Grayscale = 1,
        Sepia = 2,
    };

    /// <summary>
    /// 初期化処理
    /// </summary>
    void Initialize(DirectXCommon *dxCommon, SrvManager *srvManager, int width,
                    int height);

    /// <summary>
    /// 描画領域を更新する
    /// </summary>
    void Resize(int width, int height);

    /// <summary>
    /// 指定SRVを全画面へ描画する
    /// </summary>
    void Draw(D3D12_GPU_DESCRIPTOR_HANDLE textureHandle);

    /// <summary>
    /// ポストエフェクトの種類を設定する
    /// </summary>
    void SetMode(Mode mode);

    /// <summary>
    /// 現在のポストエフェクトの種類を取得する
    /// </summary>
    Mode GetMode() const { return mode_; }

  private:
    struct EffectConstBuffer {
        int32_t mode = 0;
        float padding[3]{};
    };

    void CreateRootSignature();
    void CreatePipelineState();
    void CreateConstantBuffer();
    void UpdateConstantBuffer();

    DirectXCommon *dxCommon_ = nullptr;
    SrvManager *srvManager_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> constBuffer_;
    EffectConstBuffer *mappedConstBuffer_ = nullptr;
    D3D12_VIEWPORT viewport_{};
    D3D12_RECT scissorRect_{};
    Mode mode_ = Mode::Grayscale;
};
