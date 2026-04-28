#include "Model.hlsli"

Texture2D tex0 : register(t0);
TextureCube<float4> gEnvironmentTexture : register(t2);
Texture2D<float4> gDissolveNoiseTexture : register(t3);
SamplerState samp0 : register(s0);

static const int kMaxPointLights = 8;

cbuffer ObjectTransform : register(b0)
{
    float4x4 matWVP;
    float4x4 matWorld;
};

cbuffer SceneParams : register(b1)
{
    float4 cameraPos;
    float4 effectColor;
    float4 effectParams;
    float4 keyLightDirection;
    float4 keyLightColor;
    float4 fillLightDirection;
    float4 fillLightColor;
    float4 ambientColor;
    PointLight pointLights[kMaxPointLights];
    float4 pointLightParams;
    float4 lightingParams;
};

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

float4 main(ModelVSOutput input) : SV_TARGET
{
    float2 uv = mul(float4(input.uv, 0.0f, 1.0f), uvTransform).xy;

    float4 texColor = float4(1, 1, 1, 1);
    if (enableTexture != 0)
    {
        texColor = tex0.Sample(samp0, uv);
    }

    float4 finalColor = texColor * color;

    float dissolveEdgeRate = 0.0f;
    if (enableDissolve != 0)
    {
        float dissolveNoise = gDissolveNoiseTexture.Sample(samp0, uv).r;
        float dissolveAmount = dissolveNoise - saturate(dissolveThreshold);
        clip(dissolveAmount);

        float edgeWidth = max(dissolveEdgeWidth, 0.0001f);
        dissolveEdgeRate = 1.0f - smoothstep(0.0f, edgeWidth, dissolveAmount);
    }

    float3 normal = normalize(input.worldNormal);
    float3 viewDir = normalize(cameraPos.xyz - input.worldPos);

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
    float keySpecular = pow(saturate(dot(normal, halfVector)), specularPower) * specularStrength;

    float rimPower = max(lightingParams.z, 0.5f);
    float rim = pow(saturate(1.0f - dot(normal, viewDir)), rimPower);

    float3 pointAccum = float3(0.0f, 0.0f, 0.0f);

    int pointLightCount = min((int)pointLightParams.x, kMaxPointLights);
    [loop]
    for (int pointLightIndex = 0; pointLightIndex < pointLightCount; ++pointLightIndex)
    {
        float3 pointVector = pointLights[pointLightIndex].positionRange.xyz - input.worldPos;
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

    float3 lighting =
        ambientColor.rgb +
        keyLightColor.rgb * keyDiffuse +
        fillLightColor.rgb * fillDiffuse +
        pointAccum +
        keyLightColor.rgb * keySpecular +
        fillLightColor.rgb * rim * fillLightColor.a;

    finalColor.rgb *= lighting;

    float3 reflectedVector = reflect(-viewDir, normal);
    uint envWidth = 0;
    uint envHeight = 0;
    uint envMipLevels = 1;
    gEnvironmentTexture.GetDimensions(0, envWidth, envHeight, envMipLevels);
    float maxMipLevel = max((float)envMipLevels - 1.0f, 0.0f);
    float mipLevel = saturate(reflectionRoughness) * maxMipLevel;
    float3 environmentColor =
        gEnvironmentTexture.SampleLevel(samp0, reflectedVector, mipLevel).rgb;
    float environmentStrength =
        reflectionStrength + rim * reflectionFresnelStrength;
    finalColor.rgb += environmentColor * environmentStrength;
    finalColor.rgb =
        lerp(finalColor.rgb, dissolveEdgeColor.rgb,
             dissolveEdgeRate * dissolveEdgeColor.a);

    float effectIntensity = effectParams.x;
    if (effectIntensity > 0.0001f)
    {
        float fresnelPower = max(effectParams.y, 0.5f);
        float noiseAmount = saturate(effectParams.z);
        float time = effectParams.w;

        float effectRim = pow(saturate(1.0f - abs(dot(normal, viewDir))),
                        fresnelPower);

        float noise =
            sin(input.worldPos.x * 10.0f + time * 15.0f) *
            sin(input.worldPos.y * 12.0f - time * 11.0f) *
            sin(input.worldPos.z * 9.0f + time * 17.0f);
        noise = lerp(1.0f, 0.65f + 0.35f * noise, noiseAmount);

        float pulse = 0.8f + 0.2f * sin(time * 20.0f + input.worldPos.y * 8.0f);
        float glow = effectRim * noise * pulse * effectIntensity;

        float bodyFade = saturate(effectIntensity * 0.38f);
        finalColor.rgb *= lerp(1.0f, 0.24f, bodyFade);
        finalColor.rgb = lerp(finalColor.rgb, finalColor.rgb * float3(0.28f, 0.08f, 0.16f),
                              bodyFade * 0.75f);

        finalColor.rgb += effectColor.rgb * glow;
        finalColor.a = saturate(finalColor.a + effectColor.a * glow * 0.55f);
    }

    return finalColor;
}
