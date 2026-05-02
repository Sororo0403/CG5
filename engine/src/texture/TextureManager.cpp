#include "TextureManager.h"
#include "DxUtils.h"
#include "ResourcePath.h"
#include "TextureGpuStore.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cwctype>
#include <filesystem>
#include <stdexcept>
#include <system_error>
#include <vector>

static std::filesystem::path ResolveTexturePath(const std::wstring &path) {
    return ResourcePath::FindExisting(std::filesystem::path(path));
}

static std::wstring NormalizePathKey(const std::filesystem::path &path) {
    std::wstring key = path.lexically_normal().wstring();

#ifdef _WIN32
    std::transform(key.begin(), key.end(), key.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(towlower(c)); });
#endif

    return key;
}

static float Fade(float t) { return t * t * (3.0f - 2.0f * t); }

static float Lerp(float a, float b, float t) { return a + (b - a) * t; }

static float HashNoise(int32_t x, int32_t y, uint32_t seed) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u +
                 static_cast<uint32_t>(y) * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13u)) * 1274126177u;
    h ^= h >> 16u;
    return static_cast<float>(h & 0x00FFFFFFu) / static_cast<float>(0x00FFFFFFu);
}

static float ValueNoise(float x, float y, uint32_t seed) {
    const int32_t x0 = static_cast<int32_t>(std::floor(x));
    const int32_t y0 = static_cast<int32_t>(std::floor(y));
    const float tx = Fade(x - static_cast<float>(x0));
    const float ty = Fade(y - static_cast<float>(y0));

    const float n00 = HashNoise(x0, y0, seed);
    const float n10 = HashNoise(x0 + 1, y0, seed);
    const float n01 = HashNoise(x0, y0 + 1, seed);
    const float n11 = HashNoise(x0 + 1, y0 + 1, seed);

    return Lerp(Lerp(n00, n10, tx), Lerp(n01, n11, tx), ty);
}

using namespace DirectX;
using namespace DxUtils;

TextureManager::TextureManager()
    : gpuStore_(std::make_unique<TextureGpuStore>()) {}

TextureManager::~TextureManager() = default;

TextureManager::TextureManager(TextureManager &&) noexcept = default;

TextureManager &
TextureManager::operator=(TextureManager &&) noexcept = default;

void TextureManager::Initialize(DirectXCommon *dxCommon,
                                SrvManager *srvManager,
                                const DirectXCommon::UploadContext &uploadContext) {
    if (!dxCommon || !srvManager) {
        throw std::invalid_argument(
            "TextureManager::Initialize requires valid managers");
    }

    gpuStore_->Initialize(dxCommon, srvManager);
    filePathToTextureId_.clear();

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

    gpuStore_->CreateTexture(uploadContext, &image, 1, metadata);
    defaultCubeTextureId_ = CreateSolidCubeTexture(uploadContext, 0xFF000000u);
}

uint32_t TextureManager::Load(const DirectXCommon::UploadContext &uploadContext,
                              const std::wstring &filePath) {
    const std::filesystem::path resolvedPath = ResolveTexturePath(filePath);
    const std::wstring pathKey = NormalizePathKey(resolvedPath);

    auto it = filePathToTextureId_.find(pathKey);
    if (it != filePathToTextureId_.end()) {
        return it->second;
    }

    ScratchImage scratch;
    TexMetadata metadata{};

    const std::wstring ext = resolvedPath.extension().wstring();

    if (_wcsicmp(ext.c_str(), L".dds") == 0) {
        ThrowIfFailed(
            LoadFromDDSFile(resolvedPath.c_str(), DDS_FLAGS_NONE, &metadata,
                            scratch),
            "LoadFromDDSFile failed");
    } else {
        ThrowIfFailed(
            LoadFromWICFile(resolvedPath.c_str(), WIC_FLAGS_NONE, &metadata,
                            scratch),
            "LoadFromWICFile failed");
    }

    uint32_t id =
        gpuStore_->CreateTexture(uploadContext, scratch.GetImages(),
                                scratch.GetImageCount(), metadata);
    filePathToTextureId_[pathKey] = id;

    return id;
}

uint32_t TextureManager::LoadFromMemory(
    const DirectXCommon::UploadContext &uploadContext, const uint8_t *data,
    size_t size) {
    if (!data || size == 0) {
        throw std::invalid_argument(
            "TextureManager::LoadFromMemory requires image data");
    }

    ScratchImage scratch;
    TexMetadata metadata{};

    ThrowIfFailed(
        LoadFromWICMemory(data, size, WIC_FLAGS_NONE, &metadata, scratch),
        "LoadFromWICMemory failed");

    uint32_t id =
        gpuStore_->CreateTexture(uploadContext, scratch.GetImages(),
                                scratch.GetImageCount(), metadata);

    return id;
}

uint32_t TextureManager::CreateNoiseTexture(
    const DirectXCommon::UploadContext &uploadContext, uint32_t width,
    uint32_t height) {
    width = (std::max)(width, 1u);
    height = (std::max)(height, 1u);

    std::vector<uint32_t> pixels(static_cast<size_t>(width) * height);
    constexpr uint32_t seed = 0xC65D1A5Bu;

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(width);
            const float v = static_cast<float>(y) / static_cast<float>(height);

            float amplitude = 0.55f;
            float frequency = 4.0f;
            float value = 0.0f;
            float totalAmplitude = 0.0f;
            for (uint32_t octave = 0; octave < 5; ++octave) {
                value += ValueNoise(u * frequency, v * frequency,
                                    seed + octave * 97u) *
                         amplitude;
                totalAmplitude += amplitude;
                amplitude *= 0.5f;
                frequency *= 2.0f;
            }

            value = (std::clamp)(value / totalAmplitude, 0.0f, 1.0f);
            const uint8_t gray = static_cast<uint8_t>(value * 255.0f);
            pixels[static_cast<size_t>(y) * width + x] =
                0xFF000000u | (static_cast<uint32_t>(gray) << 16u) |
                (static_cast<uint32_t>(gray) << 8u) |
                static_cast<uint32_t>(gray);
        }
    }

    Image image{};
    image.width = width;
    image.height = height;
    image.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    image.rowPitch = static_cast<size_t>(width) * sizeof(uint32_t);
    image.slicePitch = image.rowPitch * height;
    image.pixels = reinterpret_cast<uint8_t *>(pixels.data());

    TexMetadata metadata{};
    metadata.width = width;
    metadata.height = height;
    metadata.depth = 1;
    metadata.arraySize = 1;
    metadata.mipLevels = 1;
    metadata.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    metadata.dimension = TEX_DIMENSION_TEXTURE2D;

    return gpuStore_->CreateTexture(uploadContext, &image, 1, metadata);
}

uint32_t TextureManager::CreateSolidCubeTexture(
    const DirectXCommon::UploadContext &uploadContext, uint32_t rgba) {
    std::array<uint32_t, 6> pixels{};
    pixels.fill(rgba);

    std::array<Image, 6> images{};
    for (Image &image : images) {
        image.width = 1;
        image.height = 1;
        image.format = DXGI_FORMAT_R8G8B8A8_UNORM;
        image.rowPitch = sizeof(uint32_t);
        image.slicePitch = sizeof(uint32_t);
    }

    for (size_t index = 0; index < images.size(); ++index) {
        images[index].pixels = reinterpret_cast<uint8_t *>(&pixels[index]);
    }

    TexMetadata metadata{};
    metadata.width = 1;
    metadata.height = 1;
    metadata.depth = 1;
    metadata.arraySize = static_cast<size_t>(images.size());
    metadata.mipLevels = 1;
    metadata.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    metadata.dimension = TEX_DIMENSION_TEXTURE2D;
    metadata.miscFlags = TEX_MISC_TEXTURECUBE;

    return gpuStore_->CreateTexture(uploadContext, images.data(), images.size(),
                                   metadata);
}

void TextureManager::ReleaseUploadBuffers() { gpuStore_->ReleaseUploadBuffers(); }

D3D12_GPU_DESCRIPTOR_HANDLE
TextureManager::GetGpuHandle(uint32_t textureId) const {
    return gpuStore_->GetGpuHandle(textureId);
}

ID3D12Resource *TextureManager::GetResource(uint32_t textureId) const {
    return gpuStore_->GetResource(textureId);
}

uint32_t TextureManager::GetWidth(uint32_t id) const {
    return gpuStore_->GetWidth(id);
}

uint32_t TextureManager::GetHeight(uint32_t id) const {
    return gpuStore_->GetHeight(id);
}
