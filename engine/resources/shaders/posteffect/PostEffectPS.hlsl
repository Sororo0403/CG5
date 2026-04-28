#include "PostEffect.hlsli"

Texture2D renderTexture : register(t0);
SamplerState textureSampler : register(s0);

float4 SampleBoxFilter(PostEffectVSOutput input, int radius, float weight)
{
    float4 color = float4(0.0f, 0.0f, 0.0f, 0.0f);

    for (int y = -radius; y <= radius; ++y)
    {
        for (int x = -radius; x <= radius; ++x)
        {
            const float2 offset = float2(x, y) * texelSize;
            color +=
                renderTexture.Sample(textureSampler, input.uv + offset) * weight;
        }
    }

    return color;
}

float4 main(PostEffectVSOutput input) : SV_TARGET
{
    float4 outputColor = renderTexture.Sample(textureSampler, input.uv);

    if (filterMode == 1)
    {
        outputColor = SampleBoxFilter(input, 1, 1.0f / 9.0f);
    }
    else if (filterMode == 2)
    {
        outputColor = SampleBoxFilter(input, 2, 1.0f / 25.0f);
    }

    if (colorMode == 1)
    {
        float value = dot(outputColor.rgb, float3(0.2125f, 0.7154f, 0.0721f));
        outputColor.rgb = value.xxx;
    }
    else if (colorMode == 2)
    {
        float value = dot(outputColor.rgb, float3(0.2125f, 0.7154f, 0.0721f));
        outputColor.rgb = saturate(value.xxx * float3(1.20f, 1.00f, 0.80f));
    }

    if (enableVignetting != 0)
    {
        float2 correct = input.uv * (1.0f - input.uv.yx);
        float vignette = correct.x * correct.y * 16.0f;
        vignette = saturate(pow(vignette, 0.8f));
        outputColor.rgb *= vignette;
    }

    return outputColor;
}
