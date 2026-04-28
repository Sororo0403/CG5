#ifndef FILTER_EFFECT_HLSLI
#define FILTER_EFFECT_HLSLI

float4 SampleBoxFilter(Texture2D sourceTexture, SamplerState sourceSampler,
                       float2 uv, int radius, float weight)
{
    float4 color = float4(0.0f, 0.0f, 0.0f, 0.0f);

    for (int y = -radius; y <= radius; ++y)
    {
        for (int x = -radius; x <= radius; ++x)
        {
            const float2 offset = float2(x, y) * texelSize;
            color += sourceTexture.Sample(sourceSampler, uv + offset) * weight;
        }
    }

    return color;
}

float4 ApplyFilterEffect(Texture2D sourceTexture, SamplerState sourceSampler,
                         float2 uv, int mode)
{
    if (mode == 1)
    {
        return SampleBoxFilter(sourceTexture, sourceSampler, uv, 1,
                               1.0f / 9.0f);
    }

    if (mode == 2)
    {
        return SampleBoxFilter(sourceTexture, sourceSampler, uv, 2,
                               1.0f / 25.0f);
    }

    return sourceTexture.Sample(sourceSampler, uv);
}

#endif // FILTER_EFFECT_HLSLI
