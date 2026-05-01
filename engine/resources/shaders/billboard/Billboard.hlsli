#ifndef BILLBOARD_HLSLI
#define BILLBOARD_HLSLI

struct BillboardVSInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float shape : BLENDINDICES0;
};

struct BillboardVSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
    float shape : TEXCOORD1;
};

#endif // BILLBOARD_HLSLI
