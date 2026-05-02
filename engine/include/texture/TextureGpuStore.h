#pragma once
#include "DirectXCommon.h"
#include "Texture.h"
#include <DirectXTex.h>
#include <cstddef>
#include <cstdint>
#include <d3d12.h>
#include <vector>
#include <wrl.h>

class SrvManager;

class TextureGpuStore {
  private:
    struct Entry {
        Texture texture;
        uint32_t srvIndex = 0;
    };

  public:
    ~TextureGpuStore();

    void Initialize(DirectXCommon *dxCommon, SrvManager *srvManager);

    uint32_t CreateTexture(const DirectXCommon::UploadContext &uploadContext,
                           const DirectX::Image *images, size_t imageCount,
                           const DirectX::TexMetadata &metadata);

    void ReleaseUploadBuffers();

    D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(uint32_t textureId) const;
    ID3D12Resource *GetResource(uint32_t textureId) const;
    uint32_t GetWidth(uint32_t id) const;
    uint32_t GetHeight(uint32_t id) const;

  private:
    void ReleaseSrvs();

    DirectXCommon *dxCommon_ = nullptr;
    SrvManager *srvManager_ = nullptr;

    std::vector<Entry> textures_;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> uploadBuffers_;
};
