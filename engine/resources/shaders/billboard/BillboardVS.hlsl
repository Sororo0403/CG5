#include "Billboard.hlsli"

cbuffer BillboardCB : register(b0)
{
    float4x4 viewProjection;
};

BillboardVSOutput main(BillboardVSInput input)
{
    BillboardVSOutput output;
    output.position = mul(float4(input.position, 1.0f), viewProjection);
    output.uv = input.uv;
    output.color = input.color;
    output.shape = input.shape;
    return output;
}
