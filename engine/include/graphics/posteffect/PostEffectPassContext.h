#pragma once
#include <d3d12.h>
#include <cstdint>

struct PostEffectConstants {
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
    float radialBlurCenter[2]{0.5f, 0.5f};
    float radialBlurStrength = 0.0f;
    int32_t radialBlurSampleCount = 10;
    int32_t randomMode = 0;
    float randomStrength = 0.0f;
    float randomScale = 240.0f;
    float randomTime = 0.0f;
};

struct PostEffectPassContext {
    PostEffectConstants &constants;
    D3D12_GPU_DESCRIPTOR_HANDLE sourceTexture{};
    D3D12_GPU_DESCRIPTOR_HANDLE depthTexture{};
    int width = 1;
    int height = 1;
};
