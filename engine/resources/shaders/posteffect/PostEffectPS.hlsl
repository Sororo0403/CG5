#include "PostEffect.hlsli"

Texture2D renderTexture : register(t0);
SamplerState textureSampler : register(s0);

float4 main(PostEffectVSOutput input) : SV_TARGET
{
    float4 outputColor = renderTexture.Sample(textureSampler, input.uv);

    if (effectMode == 1)
    {
        float value = dot(outputColor.rgb, float3(0.2125f, 0.7154f, 0.0721f));
        outputColor.rgb = value.xxx;
    }
    else if (effectMode == 2)
    {
        float value = dot(outputColor.rgb, float3(0.2125f, 0.7154f, 0.0721f));
        outputColor.rgb = saturate(value.xxx * float3(1.20f, 1.00f, 0.80f));
    }

    return outputColor;
}
