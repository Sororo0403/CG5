#include "Model.hlsli"

Texture2D tex0 : register(t0);
Texture2D<float4> gDissolveNoiseTexture : register(t3);
SamplerState samp0 : register(s0);

cbuffer Material : register(b2)
{
    float4 color;
    float4x4 uvTransform;
    int enableTexture;
    float reflectionStrength;
    float reflectionFresnelStrength;
    float reflectionRoughness;
    int enableDissolve;
    float dissolveThreshold;
    float dissolveEdgeWidth;
    float materialPadding0;
    float4 dissolveEdgeColor;
};

struct GBufferOutput
{
    float4 albedo : SV_TARGET0;
    float4 normal : SV_TARGET1;
    float4 material : SV_TARGET2;
};

GBufferOutput main(ModelVSOutput input)
{
    float2 uv = mul(float4(input.uv, 0.0f, 1.0f), uvTransform).xy;

    float4 texColor = float4(1, 1, 1, 1);
    if (enableTexture != 0)
    {
        texColor = tex0.Sample(samp0, uv);
    }

    float dissolveEdgeRate = 0.0f;
    if (enableDissolve != 0)
    {
        float dissolveNoise = gDissolveNoiseTexture.Sample(samp0, uv).r;
        float dissolveAmount = dissolveNoise - saturate(dissolveThreshold);
        clip(dissolveAmount);

        float edgeWidth = max(dissolveEdgeWidth, 0.0001f);
        dissolveEdgeRate = 1.0f - smoothstep(0.0f, edgeWidth, dissolveAmount);
    }

    float4 albedo = texColor * color;
    albedo.rgb = lerp(albedo.rgb, dissolveEdgeColor.rgb,
                      dissolveEdgeRate * dissolveEdgeColor.a);

    float3 normal = normalize(input.worldNormal);

    GBufferOutput output;
    output.albedo = albedo;
    output.normal = float4(normal * 0.5f + 0.5f, 1.0f);
    output.material = float4(reflectionStrength, reflectionFresnelStrength,
                             reflectionRoughness, 0.0f);
    return output;
}
