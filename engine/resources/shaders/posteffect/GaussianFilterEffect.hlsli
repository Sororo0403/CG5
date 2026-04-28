#ifndef GAUSSIAN_FILTER_EFFECT_HLSLI
#define GAUSSIAN_FILTER_EFFECT_HLSLI

float Gaussian(float x, float y, float sigma)
{
    const float twoSigmaSquare = 2.0f * sigma * sigma;
    return exp(-(x * x + y * y) / twoSigmaSquare) /
           (3.14159265f * twoSigmaSquare);
}

float4 SampleGaussianFilter(Texture2D sourceTexture, SamplerState sourceSampler,
                            float2 uv)
{
    float weights[3][3];
    float totalWeight = 0.0f;

    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            const float weight = Gaussian((float)x, (float)y, 1.0f);
            weights[y + 1][x + 1] = weight;
            totalWeight += weight;
        }
    }

    float4 color = float4(0.0f, 0.0f, 0.0f, 0.0f);

    for (int sampleY = -1; sampleY <= 1; ++sampleY)
    {
        for (int sampleX = -1; sampleX <= 1; ++sampleX)
        {
            const float2 offset = float2(sampleX, sampleY) * texelSize;
            color += sourceTexture.Sample(sourceSampler, uv + offset) *
                     weights[sampleY + 1][sampleX + 1];
        }
    }

    return color / totalWeight;
}

#endif // GAUSSIAN_FILTER_EFFECT_HLSLI
