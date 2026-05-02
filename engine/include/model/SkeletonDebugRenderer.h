#pragma once
#include "AnimationComponents.h"
#include "Camera.h"
#include "Model.h"
#include "Transform.h"
#include <DirectXMath.h>
#include <cstdint>
#include <d3d12.h>
#include <vector>
#include <wrl.h>

class DirectXCommon;

/// <summary>
/// SkeletonのJoint親子関係をラインで可視化するデバッグ描画
/// </summary>
class SkeletonDebugRenderer {
  public:
    void Initialize(DirectXCommon *dxCommon);
    void Draw(const Model &model, const Transform &transform,
              const Camera &camera);
    void Draw(const Model &model, const Transform &transform,
              const Camera &camera, const SkeletonPoseComponent &pose);

  private:
    struct LineVertex {
        DirectX::XMFLOAT3 position{};
        DirectX::XMFLOAT4 color{};
    };

    struct ViewProjectionConstBufferData {
        DirectX::XMFLOAT4X4 matViewProjection{};
    };

    void CreateRootSignature();
    void CreatePipelineState();
    void CreateBuffers();
    void EnsureVertexCapacity(uint32_t vertexCount);
    DirectX::XMMATRIX MakeWorldMatrix(const Transform &transform) const;
    DirectX::XMMATRIX MakeWorldMatrix(const Transform &transform,
                                      const SkeletonPoseComponent &pose) const;
    void DrawInternal(const Model &model, const Transform &transform,
                      const Camera &camera,
                      const std::vector<DirectX::XMFLOAT4X4> &skeletonMatrices,
                      const SkeletonPoseComponent *pose);

  private:
    static constexpr uint32_t kInitialMaxVertices = 4096;

    DirectXCommon *dxCommon_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> viewProjectionBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    LineVertex *mappedVertices_ = nullptr;
    ViewProjectionConstBufferData *mappedViewProjection_ = nullptr;
    uint32_t vertexCapacity_ = 0;
};
