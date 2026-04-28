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
    enum class ColorMode : int32_t {
        None = 0,
        Grayscale = 1,
        Sepia = 2,
    };

    enum class FilterMode : int32_t {
        None = 0,
        Box3x3 = 1,
        Box5x5 = 2,
        Gaussian3x3 = 3,
        GaussianBlur7x7 = 4,
    };

    enum class EdgeMode : int32_t {
        None = 0,
        Luminance = 1,
        Depth = 2,
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
    void Draw(D3D12_GPU_DESCRIPTOR_HANDLE textureHandle,
              D3D12_GPU_DESCRIPTOR_HANDLE depthHandle);

    /// <summary>
    /// 色変換エフェクトの種類を設定する
    /// </summary>
    void SetColorMode(ColorMode mode);

    /// <summary>
    /// 現在の色変換エフェクトの種類を取得する
    /// </summary>
    ColorMode GetColorMode() const { return colorMode_; }

    /// <summary>
    /// 平滑化フィルターの種類を設定する
    /// </summary>
    void SetFilterMode(FilterMode mode);

    /// <summary>
    /// 現在の平滑化フィルターの種類を取得する
    /// </summary>
    FilterMode GetFilterMode() const { return filterMode_; }

    /// <summary>
    /// エッジ抽出の種類を設定する
    /// </summary>
    void SetEdgeMode(EdgeMode mode);

    /// <summary>
    /// 現在のエッジ抽出の種類を取得する
    /// </summary>
    EdgeMode GetEdgeMode() const { return edgeMode_; }

    /// <summary>
    /// 輝度エッジの閾値を設定する
    /// </summary>
    void SetLuminanceEdgeThreshold(float threshold);

    /// <summary>
    /// 輝度エッジの閾値を取得する
    /// </summary>
    float GetLuminanceEdgeThreshold() const {
        return luminanceEdgeThreshold_;
    }

    /// <summary>
    /// 深度エッジの閾値を設定する
    /// </summary>
    void SetDepthEdgeThreshold(float threshold);

    /// <summary>
    /// 深度エッジの閾値を取得する
    /// </summary>
    float GetDepthEdgeThreshold() const { return depthEdgeThreshold_; }

    /// <summary>
    /// 深度復元に使うNear/Farを設定する
    /// </summary>
    void SetDepthParameters(float nearZ, float farZ);

    /// <summary>
    /// ビネット効果の有効/無効を設定する
    /// </summary>
    void SetVignettingEnabled(bool enabled);

    /// <summary>
    /// ビネット効果が有効か取得する
    /// </summary>
    bool IsVignettingEnabled() const { return enableVignetting_; }

  private:
    struct EffectConstBuffer {
        int32_t colorMode = 0;
        int32_t enableVignetting = 0;
        int32_t filterMode = 0;
        int32_t padding0 = 0;
        float texelSize[2]{};
        float padding1[2]{};
        int32_t edgeMode = 0;
        float luminanceEdgeThreshold = 0.2f;
        float depthEdgeThreshold = 0.02f;
        float padding2 = 0.0f;
        float nearZ = 0.1f;
        float farZ = 100.0f;
        float padding3[2]{};
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
    ColorMode colorMode_ = ColorMode::None;
    FilterMode filterMode_ = FilterMode::None;
    EdgeMode edgeMode_ = EdgeMode::None;
    bool enableVignetting_ = true;
    float luminanceEdgeThreshold_ = 0.2f;
    float depthEdgeThreshold_ = 0.02f;
    float nearZ_ = 0.1f;
    float farZ_ = 100.0f;
    int width_ = 1;
    int height_ = 1;
};
