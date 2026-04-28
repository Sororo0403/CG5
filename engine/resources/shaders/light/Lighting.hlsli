#ifndef LIGHTING_HLSLI
#define LIGHTING_HLSLI

static const int kMaxPointLights = 8;

struct PointLight
{
    float4 positionRange;
    float4 colorIntensity;
};

#endif // LIGHTING_HLSLI
