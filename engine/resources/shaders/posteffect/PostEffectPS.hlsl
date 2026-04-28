#include "PostEffect.hlsli"
#include "ColorEffect.hlsli"
#include "FilterEffect.hlsli"
#include "VignettingEffect.hlsli"

Texture2D renderTexture : register(t0);
SamplerState textureSampler : register(s0);

float4 main(PostEffectVSOutput input) : SV_TARGET
{
    float4 outputColor =
        ApplyFilterEffect(renderTexture, textureSampler, input.uv, filterMode);

    outputColor.rgb = ApplyColorEffect(outputColor.rgb, colorMode);
    if (enableVignetting != 0)
    {
        outputColor.rgb = ApplyVignettingEffect(outputColor.rgb, input.uv);
    }

    return outputColor;
}
