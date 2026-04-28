#ifndef POST_EFFECT_HLSLI
#define POST_EFFECT_HLSLI

struct PostEffectVSOutput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

cbuffer PostEffectConstants : register(b0)
{
    int colorMode;
    int enableVignetting;
    int filterMode;
    int padding0;
    float2 texelSize;
    float2 padding1;
};

#endif // POST_EFFECT_HLSLI
