#include "texture/TextureManager.h"
#include "core/AssetManager.h"
#include "graphics/DirectXCommon.h"
#include "graphics/DxHelpers.h"
#include "graphics/DxUtils.h"
#include "graphics/SrvManager.h"
#include "texture/Texture.h"
#include <algorithm>
#include <chrono>
#include <cwctype>
#include <filesystem>
#include <future>
#include <stdexcept>
#include <vector>

static std::filesystem::path ResolveTexturePath(const std::wstring &path) {
    return AssetManager::ResolvePath(std::filesystem::path(path));
}

static std::wstring NormalizePathKey(const std::filesystem::path &path) {
    std::wstring key = path.lexically_normal().wstring();

#ifdef _WIN32
    std::transform(key.begin(), key.end(), key.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
#endif

    return key;
}

static TextureManager::DecodedTexture DecodeTextureFileForAsync(
    const std::wstring &filePath) {
    const std::filesystem::path resolvedPath = ResolveTexturePath(filePath);
    if (!std::filesystem::exists(resolvedPath)) {
        throw std::runtime_error("Texture file not found. requested=" +
                                 std::filesystem::path(filePath).string() +
                                 " resolved=" + resolvedPath.string());
    }

    TextureManager::DecodedTexture decoded{};
    decoded.pathKey = NormalizePathKey(resolvedPath);
    const std::wstring ext = resolvedPath.extension().wstring();

    if (_wcsicmp(ext.c_str(), L".dds") == 0) {
        const std::string message =
            "LoadFromDDSFile failed: " + resolvedPath.string();
        DxUtils::ThrowIfFailed(
            DirectX::LoadFromDDSFile(resolvedPath.c_str(),
                                      DirectX::DDS_FLAGS_NONE,
                                      &decoded.metadata, decoded.scratch),
            message.c_str());
    } else {
        const std::string message =
            "LoadFromWICFile failed: " + resolvedPath.string();
        DxUtils::ThrowIfFailed(
            DirectX::LoadFromWICFile(resolvedPath.c_str(),
                                      DirectX::WIC_FLAGS_IGNORE_SRGB,
                                      &decoded.metadata, decoded.scratch),
            message.c_str());
    }

    return decoded;
}

using namespace DirectX;
using namespace DxUtils;
using Microsoft::WRL::ComPtr;

TextureManager &TextureManager::GetInstance() {
    static TextureManager instance;
    return instance;
}

void TextureManager::Initialize(DirectXCommon *dxCommon,
                                SrvManager *srvManager) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    textures_.clear();
    uploadBuffers_.clear();
    frameUploadBuffers_.clear();
    frameUploadBuffers_.resize(dxCommon_->GetSwapChainBufferCount());
    filePathToTextureId_.clear();
    asyncRequests_.clear();
    nextAsyncRequestId_ = 1;

    uint32_t whitePixel = 0xFFFFFFFF;
    Image image{};
    image.width = 1;
    image.height = 1;
    image.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    image.rowPitch = sizeof(uint32_t);
    image.slicePitch = sizeof(uint32_t);
    image.pixels = reinterpret_cast<uint8_t *>(&whitePixel);

    TexMetadata metadata{};
    metadata.width = 1;
    metadata.height = 1;
    metadata.depth = 1;
    metadata.arraySize = 1;
    metadata.mipLevels = 1;
    metadata.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    metadata.dimension = TEX_DIMENSION_TEXTURE2D;

    whiteTextureId_ = CreateTexture(&image, 1, metadata);

    uint32_t flatNormalPixel = 0xFFFF8080;
    image.pixels = reinterpret_cast<uint8_t *>(&flatNormalPixel);
    defaultNormalTextureId_ = CreateTexture(&image, 1, metadata);
}

uint32_t TextureManager::Load(const std::wstring &filePath) {
    const std::filesystem::path resolvedPath = ResolveTexturePath(filePath);
    if (!std::filesystem::exists(resolvedPath)) {
        throw std::runtime_error("Texture file not found. requested=" +
                                 std::filesystem::path(filePath).string() +
                                 " resolved=" + resolvedPath.string());
    }

    const std::wstring pathKey = NormalizePathKey(resolvedPath);

    auto it = filePathToTextureId_.find(pathKey);
    if (it != filePathToTextureId_.end()) {
        return it->second;
    }

    ScratchImage scratch;
    TexMetadata metadata{};

    const std::wstring ext = resolvedPath.extension().wstring();

    if (_wcsicmp(ext.c_str(), L".dds") == 0) {
        const std::string message =
            "LoadFromDDSFile failed: " + resolvedPath.string();
        ThrowIfFailed(LoadFromDDSFile(resolvedPath.c_str(), DDS_FLAGS_NONE,
                                      &metadata, scratch),
                      message.c_str());
    } else {
        const std::string message =
            "LoadFromWICFile failed: " + resolvedPath.string();
        ThrowIfFailed(LoadFromWICFile(resolvedPath.c_str(),
                                      WIC_FLAGS_IGNORE_SRGB, &metadata,
                                      scratch),
                      message.c_str());
    }

    uint32_t id =
        CreateTexture(scratch.GetImages(), scratch.GetImageCount(), metadata);
    filePathToTextureId_[pathKey] = id;

    return id;
}

std::vector<uint32_t>
TextureManager::LoadBatch(const std::vector<std::wstring> &filePaths) {
    std::vector<uint32_t> textureIds;
    textureIds.reserve(filePaths.size());
    for (const std::wstring &filePath : filePaths) {
        textureIds.push_back(Load(filePath));
    }
    return textureIds;
}

uint32_t TextureManager::RequestAsyncLoad(const std::wstring &filePath) {
    const std::filesystem::path resolvedPath = ResolveTexturePath(filePath);
    const std::wstring pathKey = NormalizePathKey(resolvedPath);
    if (filePathToTextureId_.find(pathKey) != filePathToTextureId_.end()) {
        AsyncTextureRequest request{};
        request.requestId = nextAsyncRequestId_++;
        request.textureId = filePathToTextureId_[pathKey];
        request.completed = true;
        asyncRequests_.push_back(std::move(request));
        return asyncRequests_.back().requestId;
    }

    AsyncTextureRequest request{};
    request.requestId = nextAsyncRequestId_++;
    request.future = std::async(std::launch::async, [filePath]() {
        return DecodeTextureFileForAsync(filePath);
    });
    asyncRequests_.push_back(std::move(request));
    return asyncRequests_.back().requestId;
}

std::vector<uint32_t> TextureManager::RequestAsyncLoadBatch(
    const std::vector<std::wstring> &filePaths) {
    std::vector<uint32_t> requestIds;
    requestIds.reserve(filePaths.size());
    for (const std::wstring &filePath : filePaths) {
        requestIds.push_back(RequestAsyncLoad(filePath));
    }
    return requestIds;
}

void TextureManager::UpdateAsyncLoads() {
    if (!dxCommon_ || !dxCommon_->IsCommandListRecording()) {
        return;
    }

    for (AsyncTextureRequest &request : asyncRequests_) {
        if (request.completed || request.failed || !request.future.valid()) {
            continue;
        }

        if (request.future.wait_for(std::chrono::seconds(0)) !=
            std::future_status::ready) {
            continue;
        }

        try {
            DecodedTexture decoded = request.future.get();
            auto cached = filePathToTextureId_.find(decoded.pathKey);
            if (cached != filePathToTextureId_.end()) {
                request.textureId = cached->second;
            } else {
                request.textureId = CreateTexture(
                    decoded.scratch.GetImages(), decoded.scratch.GetImageCount(),
                    decoded.metadata);
                filePathToTextureId_[decoded.pathKey] = request.textureId;
            }
            request.completed = true;
        } catch (...) {
            request.failed = true;
        }
    }
}

bool TextureManager::IsAsyncLoadComplete(uint32_t requestId) const {
    for (const AsyncTextureRequest &request : asyncRequests_) {
        if (request.requestId == requestId) {
            return request.completed;
        }
    }
    return false;
}

std::optional<uint32_t>
TextureManager::GetAsyncTextureId(uint32_t requestId) const {
    for (const AsyncTextureRequest &request : asyncRequests_) {
        if (request.requestId == requestId && request.completed) {
            return request.textureId;
        }
    }
    return std::nullopt;
}

bool TextureManager::HasAsyncLoadFailed(uint32_t requestId) const {
    for (const AsyncTextureRequest &request : asyncRequests_) {
        if (request.requestId == requestId) {
            return request.failed;
        }
    }
    return false;
}

uint32_t TextureManager::LoadFromMemory(const uint8_t *data, size_t size) {
    ScratchImage scratch;
    TexMetadata metadata{};

    ThrowIfFailed(
        LoadFromWICMemory(data, size, WIC_FLAGS_IGNORE_SRGB, &metadata,
                          scratch),
        "LoadFromWICMemory failed");

    uint32_t id =
        CreateTexture(scratch.GetImages(), scratch.GetImageCount(), metadata);

    return id;
}

uint32_t TextureManager::CreateFromRgbaPixels(uint32_t width, uint32_t height,
                                              const uint8_t *pixels) {
    return CreateTexture2D(width, height, DXGI_FORMAT_R8G8B8A8_UNORM, pixels,
                           static_cast<size_t>(width) * 4u);
}

uint32_t TextureManager::CreateTexture2D(uint32_t width, uint32_t height,
                                         DXGI_FORMAT format,
                                         const uint8_t *pixels,
                                         size_t rowPitch) {
    if (width == 0 || height == 0 || !pixels || rowPitch == 0) {
        throw std::runtime_error("CreateTexture2D received invalid pixel data");
    }

    Image image{};
    image.width = width;
    image.height = height;
    image.format = format;
    image.rowPitch = rowPitch;
    image.slicePitch = rowPitch * height;
    image.pixels = const_cast<uint8_t *>(pixels);

    TexMetadata metadata{};
    metadata.width = width;
    metadata.height = height;
    metadata.depth = 1;
    metadata.arraySize = 1;
    metadata.mipLevels = 1;
    metadata.format = format;
    metadata.dimension = TEX_DIMENSION_TEXTURE2D;

    return CreateTexture(&image, 1, metadata);
}

void TextureManager::UpdateTexture2D(uint32_t textureId, const uint8_t *pixels,
                                     size_t rowPitch) {
    if (!dxCommon_ || !dxCommon_->IsCommandListRecording()) {
        return;
    }
    if (!pixels || rowPitch == 0 || textureId >= textures_.size()) {
        throw std::runtime_error("UpdateTexture2D received invalid input");
    }

    Texture &texture = textures_[textureId].texture;
    if (!texture.resource || texture.width <= 0 || texture.height <= 0) {
        throw std::runtime_error("UpdateTexture2D target texture is invalid");
    }

    const UINT frameIndex = dxCommon_->GetBackBufferIndex();
    if (frameIndex < frameUploadBuffers_.size()) {
        frameUploadBuffers_[frameIndex].clear();
    }

    D3D12_RESOURCE_DESC textureDesc = texture.resource->GetDesc();
    const size_t expectedRowPitch =
        static_cast<size_t>(texture.width) *
        DirectX::BitsPerPixel(textureDesc.Format) / 8u;
    if (rowPitch < expectedRowPitch) {
        throw std::runtime_error("UpdateTexture2D rowPitch is too small");
    }

    D3D12_SUBRESOURCE_DATA subresource{};
    subresource.pData = pixels;
    subresource.RowPitch = static_cast<LONG_PTR>(rowPitch);
    subresource.SlicePitch = static_cast<LONG_PTR>(
        rowPitch * static_cast<size_t>(texture.height));

    const UINT64 uploadSize =
        GetRequiredIntermediateSize(texture.resource.Get(), 0, 1);

    ComPtr<ID3D12Resource> uploadBuffer;
    CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
    auto uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadSize);
    ThrowIfFailed(dxCommon_->GetDevice()->CreateCommittedResource(
                      &uploadHeap, D3D12_HEAP_FLAG_NONE, &uploadDesc,
                      D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                      IID_PPV_ARGS(&uploadBuffer)),
                  "Create texture update upload buffer failed");

    ID3D12GraphicsCommandList *cmdList = dxCommon_->GetCommandList();
    auto toCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(
        texture.resource.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_COPY_DEST);
    cmdList->ResourceBarrier(1, &toCopyDest);

    UpdateSubresources(cmdList, texture.resource.Get(), uploadBuffer.Get(), 0,
                       0, 1, &subresource);

    auto toShaderResource = CD3DX12_RESOURCE_BARRIER::Transition(
        texture.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmdList->ResourceBarrier(1, &toShaderResource);

    if (frameIndex < frameUploadBuffers_.size()) {
        frameUploadBuffers_[frameIndex].push_back(uploadBuffer);
    } else {
        uploadBuffers_.push_back(uploadBuffer);
    }
}

uint32_t TextureManager::CreateTexture(const Image *images, size_t imageCount,
                                       const TexMetadata &metadata) {
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

    UpdateSubresources(cmdList, texture.resource.Get(), uploadBuffer.Get(), 0,
                       0, static_cast<UINT>(subresources.size()),
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
        srvDesc.Texture2DArray.MipLevels =
            static_cast<UINT>(metadata.mipLevels);
        srvDesc.Texture2DArray.FirstArraySlice = 0;
        srvDesc.Texture2DArray.ArraySize =
            static_cast<UINT>(metadata.arraySize);
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

    texture.width = static_cast<uint32_t>(metadata.width);
    texture.height = static_cast<uint32_t>(metadata.height);

    textures_.push_back({std::move(texture), srvIndex});

    uint32_t textureId = static_cast<uint32_t>(textures_.size() - 1);

    return textureId;
}

void TextureManager::ReleaseUploadBuffers() { uploadBuffers_.clear(); }

D3D12_GPU_DESCRIPTOR_HANDLE
TextureManager::GetGpuHandle(uint32_t textureId) const {
    return srvManager_->GetGpuHandle(textures_.at(textureId).srvIndex);
}

ID3D12Resource *TextureManager::GetResource(uint32_t textureId) const {
    return textures_.at(textureId).texture.resource.Get();
}

uint32_t TextureManager::GetWidth(uint32_t id) const {
    return textures_.at(id).texture.width;
}

uint32_t TextureManager::GetHeight(uint32_t id) const {
    return textures_.at(id).texture.height;
}
