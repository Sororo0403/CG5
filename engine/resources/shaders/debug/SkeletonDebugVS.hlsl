struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

cbuffer ViewProjection : register(b0)
{
    float4x4 matViewProjection;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.position = mul(float4(input.position, 1.0f), matViewProjection);
    output.color = input.color;
    return output;
}
