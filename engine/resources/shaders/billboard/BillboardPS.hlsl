#include "Billboard.hlsli"

float4 main(BillboardVSOutput input) : SV_TARGET
{
    float2 p = input.uv * 2.0f - 1.0f;
    float alpha = input.color.a;
    if (input.shape < 0.5f)
    {
        float radius = length(p);
        alpha *= 1.0f - smoothstep(0.82f, 1.0f, radius);
    }
    else
    {
        alpha *= 1.0f - smoothstep(0.70f, 1.0f, abs(p.y));
    }
    return float4(input.color.rgb, alpha);
}
