#pragma once
#include <array>
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

class DirectXCommon;
class SrvManager;

class GBuffer {
  public:
    enum class Target : uint32_t {
        Albedo = 0,
        Normal = 1,
        Material = 2,
        Count = 3,
    };

    ~GBuffer();

    void Initialize(DirectXCommon *dxCommon, SrvManager *srvManager, int width,
                    int height);
    void Resize(int width, int height);
    void BeginGeometryPass();
    void EndGeometryPass();

    D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(Target target) const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetRtvHandle(Target target) const;

    int GetWidth() const { return width_; }
    int GetHeight() const { return height_; }

  private:
    static constexpr uint32_t kTargetCount =
        static_cast<uint32_t>(Target::Count);

    void CreateResources();

    DirectXCommon *dxCommon_ = nullptr;
    SrvManager *srvManager_ = nullptr;

    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kTargetCount> resources_{};
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    std::array<UINT, kTargetCount> srvIndices_ = {
        UINT_MAX, UINT_MAX, UINT_MAX};
    UINT rtvDescriptorSize_ = 0;
    int width_ = 0;
    int height_ = 0;
};
