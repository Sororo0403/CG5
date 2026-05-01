#pragma once
#include "posteffect/PostEffectPassContext.h"
#include "posteffect/pass/ColorGradingPass.h"
#include "posteffect/pass/DistortionPass.h"
#include "posteffect/pass/EdgePass.h"
#include "posteffect/pass/FilterPass.h"
#include "posteffect/pass/NoisePass.h"
#include "posteffect/pass/OverlayPass.h"
#include "posteffect/pass/RadialBlurPass.h"
#include "posteffect/pass/VignettePass.h"
#include <d3d12.h>
#include <wrl.h>

class Camera;
class DirectXCommon;
class SrvManager;
class SpriteRenderer;

/// <summary>
/// テクスチャを画面全体へ描画するポストエフェクト用レンダラー
/// </summary>
class PostEffectRenderer {
  public:
    using ColorMode = ColorGradingMode;
    using FilterMode = ::FilterMode;
    using EdgeMode = ::EdgeMode;
    using RandomMode = NoiseMode;

    enum class PostEffectType {
        CounterVignette,
        DemoPlayVignette,
        WarpRingStart,
        WarpRingEnd,
        Distortion,
    };

    /// <summary>
    /// 初期化処理
    /// </summary>
    void Initialize(DirectXCommon *dxCommon, SrvManager *srvManager,
                    SpriteRenderer *spriteRenderer, int width, int height);

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
    ColorMode GetColorMode() const { return colorGradingPass_.GetMode(); }

    /// <summary>
    /// 平滑化フィルターの種類を設定する
    /// </summary>
    void SetFilterMode(FilterMode mode);

    /// <summary>
    /// 現在の平滑化フィルターの種類を取得する
    /// </summary>
    FilterMode GetFilterMode() const { return filterPass_.GetMode(); }

    /// <summary>
    /// エッジ抽出の種類を設定する
    /// </summary>
    void SetEdgeMode(EdgeMode mode);

    /// <summary>
    /// 現在のエッジ抽出の種類を取得する
    /// </summary>
    EdgeMode GetEdgeMode() const { return edgePass_.GetMode(); }

    /// <summary>
    /// 輝度エッジの閾値を設定する
    /// </summary>
    void SetLuminanceEdgeThreshold(float threshold);

    /// <summary>
    /// 輝度エッジの閾値を取得する
    /// </summary>
    float GetLuminanceEdgeThreshold() const {
        return edgePass_.GetLuminanceThreshold();
    }

    /// <summary>
    /// 深度エッジの閾値を設定する
    /// </summary>
    void SetDepthEdgeThreshold(float threshold);

    /// <summary>
    /// 深度エッジの閾値を取得する
    /// </summary>
    float GetDepthEdgeThreshold() const { return edgePass_.GetDepthThreshold(); }

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
    bool IsVignettingEnabled() const { return vignettePass_.IsEnabled(); }

    /// <summary>
    /// ラジアルブラーの中心座標を設定する
    /// </summary>
    void SetRadialBlurCenter(float x, float y);

    /// <summary>
    /// ラジアルブラーの中心座標を取得する
    /// </summary>
    const float *GetRadialBlurCenter() const {
        return radialBlurPass_.GetCenter();
    }

    /// <summary>
    /// ラジアルブラーの強さを設定する
    /// </summary>
    void SetRadialBlurStrength(float strength);

    /// <summary>
    /// ラジアルブラーの強さを取得する
    /// </summary>
    float GetRadialBlurStrength() const { return radialBlurPass_.GetStrength(); }

    /// <summary>
    /// ラジアルブラーのサンプル数を設定する
    /// </summary>
    void SetRadialBlurSampleCount(int32_t sampleCount);

    /// <summary>
    /// ラジアルブラーのサンプル数を取得する
    /// </summary>
    int32_t GetRadialBlurSampleCount() const {
        return radialBlurPass_.GetSampleCount();
    }

    /// <summary>
    /// 疑似乱数ノイズの表示方法を設定する
    /// </summary>
    void SetRandomMode(RandomMode mode);

    /// <summary>
    /// 疑似乱数ノイズの表示方法を取得する
    /// </summary>
    RandomMode GetRandomMode() const { return noisePass_.GetMode(); }

    /// <summary>
    /// ノイズを重ねる強さを設定する
    /// </summary>
    void SetRandomStrength(float strength);

    /// <summary>
    /// ノイズを重ねる強さを取得する
    /// </summary>
    float GetRandomStrength() const { return noisePass_.GetStrength(); }

    /// <summary>
    /// ノイズの粒の細かさを設定する
    /// </summary>
    void SetRandomScale(float scale);

    /// <summary>
    /// ノイズの粒の細かさを取得する
    /// </summary>
    float GetRandomScale() const { return noisePass_.GetScale(); }

    /// <summary>
    /// ノイズ生成に使う時間を設定する
    /// </summary>
    void SetRandomTime(float time);

    void Request(PostEffectType type);
    void Request(PostEffectType type, const DirectX::XMFLOAT3 &worldPosition);
    void Request(PostEffectType type, const DistortionEffectParams &params);
    void SetCounterVignetteActive(bool active);
    void SetDemoPlayIndicatorVisible(bool visible);
    void UpdateScreenEffects(float deltaTime, const Camera &camera, int width,
                             int height);
    void DrawScreenOverlays() const;
    const ElectricRingParamGPU &GetElectricRingParam() const;

  private:
    void CreateRootSignature();
    void CreatePipelineState();
    void CreateConstantBuffer();
    void UpdateConstantBuffer(const PostEffectConstants &constants);
    PostEffectConstants BuildConstants(D3D12_GPU_DESCRIPTOR_HANDLE textureHandle,
                                       D3D12_GPU_DESCRIPTOR_HANDLE depthHandle);

    DirectXCommon *dxCommon_ = nullptr;
    SrvManager *srvManager_ = nullptr;
    SpriteRenderer *spriteRenderer_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> constBuffer_;
    PostEffectConstants *mappedConstBuffer_ = nullptr;
    D3D12_VIEWPORT viewport_{};
    D3D12_RECT scissorRect_{};
    int width_ = 1;
    int height_ = 1;

    ColorGradingPass colorGradingPass_{};
    FilterPass filterPass_{};
    EdgePass edgePass_{};
    NoisePass noisePass_{};
    RadialBlurPass radialBlurPass_{};
    VignettePass vignettePass_{};
    DistortionPass distortionPass_{};
    OverlayPass overlayPass_{};
};
