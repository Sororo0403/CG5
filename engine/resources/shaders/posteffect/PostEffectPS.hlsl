#include "PostEffect.hlsli"
#include "ColorEffect.hlsli"
#include "EdgeEffect.hlsli"
#include "FilterEffect.hlsli"
#include "BloomEffect.hlsli"
#include "NoiseEffect.hlsli"
#include "TonemapEffect.hlsli"

Texture2D renderTexture : register(t0);
Texture2D depthTexture : register(t1);
SamplerState textureSampler : register(s0);

float3 ApplyNoise(float3 color, float2 uv)
{
    if (noiseEnabled == 0)
    {
        return color;
    }

    float noise = NoiseHash(uv * noiseScale + noiseTime);
    return saturate(color + (noise - 0.5f) * noiseStrength);
}

float4 main(PostEffectVSOutput input) : SV_TARGET
{
    float4 outputColor =
        ApplyFilterEffect(renderTexture, textureSampler, input.uv, filterMode);

    outputColor.rgb =
        ApplyColorEffect(outputColor.rgb, colorMode, grayscaleWeights);

    outputColor = ApplyEdgeEffect(outputColor, renderTexture, depthTexture,
                                  textureSampler, input.uv, edgeMode);

    outputColor.rgb = ApplyBloomEffect(renderTexture, textureSampler,
                                       outputColor.rgb, input.uv);
    outputColor.rgb = ApplyTonemapEffect(outputColor.rgb);
    outputColor.rgb = ApplyNoise(outputColor.rgb, input.uv);

    return outputColor;
}
