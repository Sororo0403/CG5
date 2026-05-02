#include "TextureGpuStore.h"
#include "DxHelpers.h"
#include "DxUtils.h"
#include "SrvManager.h"
#include <stdexcept>

using namespace DirectX;
using namespace DxUtils;
using Microsoft::WRL::ComPtr;

TextureGpuStore::~TextureGpuStore() { ReleaseSrvs(); }

void TextureGpuStore::Initialize(DirectXCommon *dxCommon,
                                 SrvManager *srvManager) {
    if (!dxCommon || !srvManager) {
        throw std::invalid_argument(
            "TextureGpuStore::Initialize requires valid managers");
    }

    ReleaseSrvs();

    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    textures_.clear();
    uploadBuffers_.clear();
}

uint32_t TextureGpuStore::CreateTexture(
    const DirectXCommon::UploadContext &uploadContext, const Image *images,
    size_t imageCount, const TexMetadata &metadata) {
    if (!dxCommon_ || !srvManager_) {
        throw std::runtime_error("TextureGpuStore is not initialized");
    }
    if (!images || imageCount == 0) {
        throw std::invalid_argument(
            "TextureGpuStore::CreateTexture requires image data");
    }
    if (!uploadContext.IsActive() || uploadContext.GetOwner() != dxCommon_) {
        throw std::runtime_error(
            "TextureGpuStore::CreateTexture requires an active upload context");
    }
    if (metadata.width == 0 || metadata.height == 0 ||
        metadata.arraySize == 0 || metadata.mipLevels == 0) {
        throw std::invalid_argument("Texture metadata has an invalid size");
    }

    Texture texture;

    auto texDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        metadata.format, static_cast<UINT64>(metadata.width),
        static_cast<UINT>(metadata.height),
        static_cast<UINT16>(metadata.arraySize),
        static_cast<UINT16>(metadata.mipLevels));

    CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);

    ThrowIfFailed(dxCommon_->GetDevice()->CreateCommittedResource(
                      &defaultHeap, D3D12_HEAP_FLAG_NONE, &texDesc,
                      D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
                      IID_PPV_ARGS(&texture.resource)),
                  "Create texture resource failed");

    std::vector<D3D12_SUBRESOURCE_DATA> subresources(imageCount);
    for (size_t imageIndex = 0; imageIndex < imageCount; ++imageIndex) {
        subresources[imageIndex].pData = images[imageIndex].pixels;
        subresources[imageIndex].RowPitch = images[imageIndex].rowPitch;
        subresources[imageIndex].SlicePitch = images[imageIndex].slicePitch;
    }

    UINT64 uploadSize = GetRequiredIntermediateSize(
        texture.resource.Get(), 0, static_cast<UINT>(subresources.size()));

    ComPtr<ID3D12Resource> uploadBuffer;

    CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
    auto uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);

    ThrowIfFailed(dxCommon_->GetDevice()->CreateCommittedResource(
                      &uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDesc,
                      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                      IID_PPV_ARGS(&uploadBuffer)),
                  "Create upload buffer failed");

    ID3D12GraphicsCommandList *cmdList = dxCommon_->GetCommandList();

    UpdateSubresources(cmdList, texture.resource.Get(), uploadBuffer.Get(), 0, 0,
                       static_cast<UINT>(subresources.size()),
                       subresources.data());

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        texture.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    cmdList->ResourceBarrier(1, &barrier);

    uploadBuffers_.push_back(uploadBuffer);

    uint32_t srvIndex = srvManager_->Allocate();

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    srvDesc.Format = metadata.format;
    if (metadata.IsCubemap()) {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srvDesc.TextureCube.MostDetailedMip = 0;
        srvDesc.TextureCube.MipLevels = static_cast<UINT>(metadata.mipLevels);
        srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
    } else if (metadata.dimension == TEX_DIMENSION_TEXTURE2D &&
               metadata.arraySize > 1) {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        srvDesc.Texture2DArray.MostDetailedMip = 0;
        srvDesc.Texture2DArray.MipLevels = static_cast<UINT>(metadata.mipLevels);
        srvDesc.Texture2DArray.FirstArraySlice = 0;
        srvDesc.Texture2DArray.ArraySize = static_cast<UINT>(metadata.arraySize);
        srvDesc.Texture2DArray.PlaneSlice = 0;
        srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
    } else {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = static_cast<UINT>(metadata.mipLevels);
        srvDesc.Texture2D.PlaneSlice = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    }

    dxCommon_->GetDevice()->CreateShaderResourceView(
        texture.resource.Get(), &srvDesc, srvManager_->GetCpuHandle(srvIndex));

    texture.width = static_cast<int>(metadata.width);
    texture.height = static_cast<int>(metadata.height);

    textures_.push_back({std::move(texture), srvIndex});
    return static_cast<uint32_t>(textures_.size() - 1);
}

void TextureGpuStore::ReleaseUploadBuffers() { uploadBuffers_.clear(); }

D3D12_GPU_DESCRIPTOR_HANDLE
TextureGpuStore::GetGpuHandle(uint32_t textureId) const {
    return srvManager_->GetGpuHandle(textures_.at(textureId).srvIndex);
}

ID3D12Resource *TextureGpuStore::GetResource(uint32_t textureId) const {
    return textures_.at(textureId).texture.resource.Get();
}

uint32_t TextureGpuStore::GetWidth(uint32_t id) const {
    return static_cast<uint32_t>(textures_.at(id).texture.width);
}

uint32_t TextureGpuStore::GetHeight(uint32_t id) const {
    return static_cast<uint32_t>(textures_.at(id).texture.height);
}

void TextureGpuStore::ReleaseSrvs() {
    if (!srvManager_) {
        return;
    }

    for (const Entry &entry : textures_) {
        srvManager_->Free(entry.srvIndex);
    }
}
