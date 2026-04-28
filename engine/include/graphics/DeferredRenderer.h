#pragma once
#include "LightManager.h"
#include <DirectXMath.h>
#include <d3d12.h>
#include <wrl.h>

class Camera;
class DirectXCommon;
class GBuffer;
class SrvManager;

class DeferredRenderer {
  public:
    void Initialize(DirectXCommon *dxCommon, SrvManager *srvManager, int width,
                    int height);
    void Resize(int width, int height);
    void Draw(const GBuffer &gBuffer, D3D12_GPU_DESCRIPTOR_HANDLE depthHandle,
              const Camera &camera, const SceneLighting &lighting);

  private:
    struct DeferredConstBuffer {
        DirectX::XMFLOAT4X4 invViewProj;
        DirectX::XMFLOAT4 cameraPos;
        ForwardLightingData lighting;
    };

    void CreateRootSignature();
    void CreatePipelineState();
    void CreateConstantBuffer();
    void UpdateConstantBuffer(const Camera &camera,
                              const SceneLighting &lighting);

    DirectXCommon *dxCommon_ = nullptr;
    SrvManager *srvManager_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> constBuffer_;
    DeferredConstBuffer *mappedConstBuffer_ = nullptr;
    D3D12_VIEWPORT viewport_{};
    D3D12_RECT scissorRect_{};
    int width_ = 1;
    int height_ = 1;
};
