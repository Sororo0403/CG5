#pragma once
#include "Camera.h"
#include <DirectXMath.h>
#include <d3d12.h>
#include <wrl.h>

class DirectXCommon;

class BillboardRenderer {
  public:
    void Initialize(DirectXCommon *dxCommon);
    void PreDraw(const Camera &camera);
    void PostDraw();
    void DrawDisc(const DirectX::XMFLOAT3 &center, float radius,
                  const DirectX::XMFLOAT4 &color, float roll = 0.0f);
    void DrawBeam(const DirectX::XMFLOAT3 &start,
                  const DirectX::XMFLOAT3 &end, float width,
                  const DirectX::XMFLOAT4 &color);

  private:
    struct Vertex {
        DirectX::XMFLOAT3 position{};
        DirectX::XMFLOAT2 uv{};
        DirectX::XMFLOAT4 color{1.0f, 1.0f, 1.0f, 1.0f};
        float shape = 0.0f;
    };

    struct ConstantBufferData {
        DirectX::XMFLOAT4X4 viewProjection{};
    };

    void CreateRootSignature();
    void CreatePipelineState();
    void CreateVertexBuffer();
    void CreateConstantBuffer();
    void DrawQuad(const Vertex (&vertices)[6]);
    DirectX::XMFLOAT3 GetCameraRight() const { return cameraRight_; }
    DirectX::XMFLOAT3 GetCameraUp() const { return cameraUp_; }
    DirectX::XMFLOAT3 GetCameraForward() const { return cameraForward_; }

    DirectXCommon *dxCommon_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    DirectX::XMFLOAT3 cameraRight_{1.0f, 0.0f, 0.0f};
    DirectX::XMFLOAT3 cameraUp_{0.0f, 1.0f, 0.0f};
    DirectX::XMFLOAT3 cameraForward_{0.0f, 0.0f, 1.0f};
};
