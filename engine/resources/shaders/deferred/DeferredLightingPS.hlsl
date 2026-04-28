#include "../posteffect/PostEffect.hlsli"
#include "../light/Lighting.hlsli"

Texture2D<float4> gAlbedoTexture : register(t0);
Texture2D<float4> gNormalTexture : register(t1);
Texture2D<float4> gMaterialTexture : register(t2);
Texture2D<float> gDepthTexture : register(t3);
SamplerState samp0 : register(s0);

cbuffer DeferredParams : register(b0)
{
    float4x4 invViewProj;
    float4 cameraPos;
    float4 keyLightDirection;
    float4 keyLightColor;
    float4 fillLightDirection;
    float4 fillLightColor;
    float4 ambientColor;
    PointLight pointLights[kMaxPointLights];
    float4 pointLightParams;
    float4 lightingParams;
};

float3 RestoreWorldPosition(float2 uv, float depth)
{
    float2 ndc = uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
    float4 clipPos = float4(ndc, depth, 1.0f);
    float4 worldPos = mul(clipPos, invViewProj);
    return worldPos.xyz / max(worldPos.w, 0.0001f);
}

float4 main(PostEffectVSOutput input) : SV_TARGET
{
    float4 albedo = gAlbedoTexture.Sample(samp0, input.uv);
    float3 normal = normalize(gNormalTexture.Sample(samp0, input.uv).xyz * 2.0f - 1.0f);
    float4 material = gMaterialTexture.Sample(samp0, input.uv);
    float depth = gDepthTexture.Sample(samp0, input.uv);

    if (depth >= 0.99999f)
    {
        discard;
    }

    float3 worldPos = RestoreWorldPosition(input.uv, depth);
    float3 viewDir = normalize(cameraPos.xyz - worldPos);

    float3 keyDir = normalize(-keyLightDirection.xyz);
    float3 fillDir = normalize(-fillLightDirection.xyz);

    float keyDiffuse = saturate(dot(normal, keyDir));
    float fillDiffuse = saturate(dot(normal, fillDir));
    float wrap = saturate(lightingParams.w);
    keyDiffuse = saturate((keyDiffuse + wrap) / (1.0f + wrap));
    fillDiffuse = saturate((fillDiffuse + wrap * 0.5f) / (1.0f + wrap * 0.5f));

    float3 halfVector = normalize(keyDir + viewDir);
    float specularPower = max(lightingParams.x, 1.0f);
    float specularStrength = saturate(lightingParams.y);
    float keySpecular = pow(saturate(dot(normal, halfVector)), specularPower) *
                        specularStrength;

    float rimPower = max(lightingParams.z, 0.5f);
    float rim = pow(saturate(1.0f - dot(normal, viewDir)), rimPower);

    float3 pointAccum = float3(0.0f, 0.0f, 0.0f);
    int pointLightCount = min((int)pointLightParams.x, kMaxPointLights);
    [loop]
    for (int pointLightIndex = 0; pointLightIndex < pointLightCount; ++pointLightIndex)
    {
        float3 pointVector = pointLights[pointLightIndex].positionRange.xyz - worldPos;
        float pointDistance = length(pointVector);
        if (pointDistance > 0.0001f)
        {
            float3 pointDir = pointVector / pointDistance;
            float pointAttenuation =
                saturate(1.0f - pointDistance / max(pointLights[pointLightIndex].positionRange.w, 0.001f));
            pointAttenuation *= pointAttenuation;
            float pointDiffuse = saturate(dot(normal, pointDir));
            pointAccum += pointLights[pointLightIndex].colorIntensity.rgb *
                          pointDiffuse * pointAttenuation *
                          pointLights[pointLightIndex].colorIntensity.w;
        }
    }

    float reflectionStrength = saturate(material.x + material.y);
    float3 lighting =
        ambientColor.rgb +
        keyLightColor.rgb * keyDiffuse +
        fillLightColor.rgb * fillDiffuse +
        pointAccum +
        keyLightColor.rgb * keySpecular * reflectionStrength +
        fillLightColor.rgb * rim * fillLightColor.a;

    return float4(albedo.rgb * lighting, albedo.a);
}
